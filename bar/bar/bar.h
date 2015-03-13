/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver main program
* Systems: all
*
\***********************************************************************/

#ifndef __BAR__
#define __BAR__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

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
#include "index.h"
#include "bar_global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file name extensions
#define FILE_NAME_EXTENSION_ARCHIVE_FILE     ".bar"
#define FILE_NAME_EXTENSION_INCREMENTAL_FILE ".bid"

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
  LOG_TYPE_STORAGE             = (1 <<  8),
  LOG_TYPE_INDEX               = (1 <<  9),
} LogTypes;

#define LOG_TYPE_NONE 0x00000000
#define LOG_TYPE_ALL  0xFFFFffff

#define MAX_CONNECTION_COUNT_UNLIMITED MAX_INT
#define MAX_STORAGE_SIZE_UNLIMITED     MAX_INT64

/***************************** Datatypes *******************************/

typedef struct
{
  String saveLine;
  String lastOutputLine;
} ConsoleSave;

/***************************** Variables *******************************/
extern GlobalOptions globalOptions;          // global options
extern String        tmpDirectory;           // temporary directory
extern IndexHandle   *indexHandle;           // index handle
extern Semaphore     consoleLock;            // lock console

/****************************** Macros *********************************/

// return short number of bytes
#define BYTES_SHORT(n) (((n)>(1024LL*1024LL*1024LL))?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        ((n)>       (1024LL*1024LL))?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        ((n)>                1024LL)?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        (double)(n) \
                       )
// return unit for short number of bytes
#define BYTES_UNIT(n) (((n)>(1024LL*1024LL*1024LL))?"GB": \
                       ((n)>       (1024LL*1024LL))?"MB": \
                       ((n)>                1024LL)?"KB": \
                       "bytes" \
                      )

#define CONSOLE_LOCKED_DO(semaphoreLock,semaphore,semaphoreLockType) \
  for (semaphoreLock = lockConsole(); \
       semaphoreLock; \
       unlockConsole(semaphore), semaphoreLock = FALSE \
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
* Name   : isPrintInfo
* Purpose: check if info should be printed
* Input  : verboseLevel - verbosity level
* Output : -
* Return : true iff info should be printed
* Notes  : -
\***********************************************************************/

bool isPrintInfo(uint verboseLevel);

/***********************************************************************\
* Name   : vprintInfo, pprintInfo, printInfo
* Purpose: output info to console
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
void pprintInfo(uint verboseLevel, const char *prefix, const char *format, ...);
void printInfo(uint verboseLevel, const char *format, ...);

/***********************************************************************\
* Name   : vlogMessage, plogMessage, logMessage
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
void plogMessage(ulong logType, const char *prefix, const char *text, ...);
void logMessage(ulong logType, const char *text, ...);

/***********************************************************************\
* Name   : lockConsole
* Purpose: lock console
* Input  : -
* Output : -
* Return : TRUE iff locked
* Notes  : -
\***********************************************************************/

bool lockConsole(void);

/***********************************************************************\
* Name   : unlockConsole
* Purpose: unlock console
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void unlockConsole(void);

/***********************************************************************\
* Name   : saveConsole
* Purpose: save and clear current console line
* Input  : file - stdout or stderr
* Output : -
* Return : saved console line
* Notes  : -
\***********************************************************************/

void saveConsole(FILE *file, String *saveLine);

/***********************************************************************\
* Name   : restoreConsole
* Purpose: restore saved console line
* Input  : file     - stdout or stderr
*          saveLine - saved console line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void restoreConsole(FILE *file, const String *saveLine);

/***********************************************************************\
* Name   : printConsole
* Purpose: output to console
* Input  : file         - stdout or stderr
*          format       - format string (like printf)
*          ...          - optional arguments (like printf)
*          arguments    - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printConsole(FILE *file, const char *format, ...);

/***********************************************************************\
* Name   : printWarning
* Purpose: output warning on console
* Input  : text - format string (like printf)
*          ...  - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printWarning(const char *text, ...);

/***********************************************************************\
* Name   : printError
* Purpose: print error message on stderr
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
* Name   : executeIOOutput
* Purpose: process exec output
* Input  : userData - string list or NULL
*          line     - line to output and to append to strin glist (if
*                     userData is not NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void executeIOOutput(void         *userData,
                     const String line
                    );

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
* Name   : initDuplicateJobOptions
* Purpose: init duplicated job options structure
* Input  : jobOptions     - job options variable
*          fromJobOptions - source job options
* Output : jobOptions - initialized job options variable
* Return : -
* Notes  : -
\***********************************************************************/

void initDuplicateJobOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions);

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
* Name   : doneJobOptions
* Purpose: done job options structure
* Input  : jobOptions - job options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneJobOptions(JobOptions *jobOptions);

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
* Name   : getFTPServerSettings
* Purpose: get FTP server settings
* Input  : hostName   - FTP server host name
*          jobOptions - job options
* Output : ftpServer - FTP server settings from job options, server
*                      list or default FTP server values
* Return : server
* Notes  : -
\***********************************************************************/

Server *getFTPServerSettings(const String     hostName,
                             const JobOptions *jobOptions,
                             FTPServer        *ftpServer
                            );

/***********************************************************************\
* Name   : getSSHServerSettings
* Purpose: get SSH server settings
* Input  : hostName   - SSH server host name
*          jobOptions - job options
* Output : sshServer  - SSH server settings from job options, server
*                       list or default SSH server values
* Return : server
* Notes  : -
\***********************************************************************/

Server *getSSHServerSettings(const String     hostName,
                             const JobOptions *jobOptions,
                             SSHServer        *sshServer
                            );

/***********************************************************************\
* Name   : getWebDAVServerSettings
* Purpose: get WebDAV server settings
* Input  : hostName   - WebDAV server host name
*          jobOptions - job options
* Output : webDAVServer - WebDAV server settings from job options,
*                         server list or default WebDAV server values
* Return : server
* Notes  : -
\***********************************************************************/

Server *getWebDAVServerSettings(const String     hostName,
                                          const JobOptions *jobOptions,
                                          WebDAVServer     *webDAVServer
                                         );

/***********************************************************************\
* Name   : getCDSettings
* Purpose: get CD settings
* Input  : jobOptions - job options
* Output : cd - cd settings from job options or default CD values
* Return : -
* Notes  : -
\***********************************************************************/

void getCDSettings(const JobOptions *jobOptions,
                   OpticalDisk      *cd
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
                    OpticalDisk      *dvd
                   );

/***********************************************************************\
* Name   : getDVDSettings
* Purpose: get DVD settings
* Input  : jobOptions - job options
* Output : bd - bd settings from job options or default BD values
* Return : -
* Notes  : -
\***********************************************************************/

void getBDSettings(const JobOptions *jobOptions,
                   OpticalDisk      *bd
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
* Name   : allocateServer
* Purpose: allocate server
* Input  : server   - server
*          priority - server connection priority; see
*                     SERVER_CONNECTION_PRIORITY_...
*          timeout  - timeout or -1 [ms]
* Output : -
* Return : TRUE iff server allocated, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool allocateServer(Server *server, ServerConnectionPriorities priority, long timeout);

/***********************************************************************\
* Name   : freeServer
* Purpose: free allocated server
* Input  : server - server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeServer(Server *server);

/***********************************************************************\
* Name   : isServerAllocationPending
* Purpose: check if a server allocation with high priority is pending
* Input  : server - server
* Output : -
* Return : TRUE if server allocation with high priority is pending,
*          FALSE otherwise
* Notes  : -
\***********************************************************************/

bool isServerAllocationPending(Server *server);

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
* Name   : parseWeekDaySet
* Purpose: parse date week day set
* Input  : names - day names to parse
* Output : weekDaySet - week day set
* Return : TRUE iff week day parsed
* Notes  : -
\***********************************************************************/

bool parseWeekDaySet(const char *names, WeekDaySet *weekDaySet);

/***********************************************************************\
* Name   : parseDateTimeNumber
* Purpose: parse date/time number (year, day, month, hour, minute)
* Input  : s - string to parse
* Output : n - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

bool parseDateTimeNumber(const String s, int *n);

/***********************************************************************\
* Name   : parseDateMonth
* Purpose: parse date month name
* Input  : s - string to parse
* Output : month - month (MONTH_JAN..MONTH_DEC)
* Return : TRUE iff month parsed
* Notes  : -
\***********************************************************************/

bool parseDateMonth(const String s, int *month);

/***********************************************************************\
* Name   : configValueParseBandWidth
* Purpose: config value call back for parsing band width setting
*          patterns
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseBandWidth(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatInitOwner
* Purpose: init format of config band width settings
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitBandWidth(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneBandWidth
* Purpose: done format of config band width setting
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneBandWidth(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatBandWidth
* Purpose: format next config band width setting
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatBandWidth(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseOwner
* Purpose: config value call back for parsing owner
*          patterns
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseOwner(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

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
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseFileEntry(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);
bool configValueParseImageEntry(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

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
* Name   : configValueParsePattern
* Purpose: config value option call back for parsing pattern
*          patterns
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParsePattern(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatInitPattern
* Purpose: init format of config pattern statements
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitPattern(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDonePattern
* Purpose: done format of config pattern statements
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDonePattern(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatPattern
* Purpose: format next config pattern statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatPattern(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseString
* Purpose: config value option call back for parsing string
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseString(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueParsePassword
* Purpose: config value option call back for parsing password
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParsePassword(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

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
* Name   : configValueFormatDonePassword
* Purpose: done format of config password setting
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDonePassword(void **formatUserData, void *userData);

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
* Name   : configValueParseDeltaCompressAlgorithm
* Purpose: config value option call back for parsing delta compress
*          algorithm
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseDeltaCompressAlgorithm(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitDeltaCompressAlgorithm
* Purpose: init format config compress algorithm
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitDeltaCompressAlgorithm(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneDeltaCompressAlgorithm
* Purpose: done format of config compress algorithm
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneDeltaCompressAlgorithm(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatDeltaCompressAlgorithm
* Purpose: format compress algorithm config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatDeltaCompressAlgorithm(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseByteCompressAlgorithm
* Purpose: config value option call back for parsing byte compress
*          algorithm
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseByteCompressAlgorithm(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitByteCompressAlgorithm
* Purpose: init format config compress algorithm
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitByteCompressAlgorithm(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneByteCompressAlgorithm
* Purpose: done format of config compress algorithm
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneByteCompressAlgorithm(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatByteCompressAlgorithm
* Purpose: format compress algorithm config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatByteCompressAlgorithm(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseCompressAlgorithm
* Purpose: config value option call back for parsing compress algorithm
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseCompressAlgorithm(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueFormatInitCompressAlgorithm
* Purpose: init format config compress algorithm
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitCompressAlgorithm(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneCompressAlgorithm
* Purpose: done format of config compress algorithm
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneCompressAlgorithm(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatCompressAlgorithm
* Purpose: format compress algorithm config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatCompressAlgorithm(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : archiveTypeToString
* Purpose: get name for archive type
* Input  : archiveType  - archive type
*          defaultValue - default value
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

const char *archiveTypeToString(ArchiveTypes archiveType, const char *defaultValue);

/***********************************************************************\
* Name   : parseArchiveType
* Purpose: parse archive type
* Input  : name - normal|full|incremental|differential
* Output : archiveType - archive type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool parseArchiveType(const char *name, ArchiveTypes *archiveType);

#ifdef __cplusplus
  }
#endif

#endif /* __BAR__ */

/* end of file */
