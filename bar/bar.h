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
#include <locale.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/configvalues.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/passwords.h"
#include "common/misc.h"
#include "common/threadpools.h"

#include "entrylists.h"
#include "compress.h"
#include "crypt.h"
#include "index.h"
#include "jobs.h"
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

/***************************** Datatypes *******************************/

typedef struct
{
  String saveLine;
  String lastOutputLine;
} ConsoleSave;

// template expand handle
typedef struct
{
  const char       *templateString;
  ExpandMacroModes expandMacroMode;
  uint64           dateTime;
  const TextMacro  *textMacros;
  uint             textMacroCount;
} TemplateHandle;

/***************************** Variables *******************************/
extern String     tmpDirectory;           // temporary directory
extern Semaphore  consoleLock;            // lock console
#ifdef HAVE_NEWLOCALE
  extern locale_t POSIXLocale;            // POSIX locale
#endif /* HAVE_NEWLOCALE */

extern ThreadPool clientThreadPool;
extern ThreadPool workerThreadPool;

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : BYTES_SHORT
* Purpose: return short number of bytes
* Input  : n - number
* Output : -
* Return : short number
* Notes  : -
\***********************************************************************/

#define BYTES_SHORT(n) (((n)>(1024LL*1024LL*1024LL*1024LL))?(double)(n)/(double)(1024LL*1024LL*1024LL*1024LL): \
                        ((n)>(       1024LL*1024LL*1024LL))?(double)(n)/(double)(       1024LL*1024LL*1024LL): \
                        ((n)>(              1024LL*1024LL))?(double)(n)/(double)(              1024LL*1024LL): \
                        ((n)>                      1024LL )?(double)(n)/(double)(                     1024LL): \
                        (double)(n) \
                       )

/***********************************************************************\
* Name   : BYTES_UNIT
* Purpose: return unit for short number of bytes
* Input  : n - number
* Output : -
* Return : unit string
* Notes  : -
\***********************************************************************/

#define BYTES_UNIT(n) (((n)>(1024LL*1024LL*1024LL*1024LL))?"TB": \
                       ((n)>(       1024LL*1024LL*1024LL))?"GB": \
                       ((n)>(              1024LL*1024LL))?"MB": \
                       ((n)>                      1024LL )?"KB": \
                       "bytes" \
                      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getConfigFileName
* Purpose: get writable config file name
* Input  : fileName - file name variable
* Output : fileName - file anme
* Return : file name
* Notes  : -
\***********************************************************************/

String getConfigFileName(String fileName);

/***********************************************************************\
* Name   : updateConfig
* Purpose: update config file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors updateConfig(void);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : getArchiveTypeText
* Purpose: get archive type text
* Input  : archiveType - archive type
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

const char *getArchiveTypeText(ArchiveTypes archiveType);

/***********************************************************************\
* Name   : getArchiveTypeShortText
* Purpose: get archive type short text
* Input  : archiveType - archive type
* Output : -
* Return : short name
* Notes  : -
\***********************************************************************/

const char *getArchiveTypeShortText(ArchiveTypes archiveType);

/***********************************************************************\
* Name   : getPasswordTypeName
* Purpose: get password type text
* Input  : passwordType - password type
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

const char *getPasswordTypeText(PasswordTypes passwordType);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : isPrintInfo
* Purpose: check if info should be printed
* Input  : verboseLevel - verbosity level
* Output : -
* Return : true iff info should be printed
* Notes  : -
\***********************************************************************/

INLINE bool isPrintInfo(uint verboseLevel);
#if defined(NDEBUG) || defined(__BAR_IMPLEMENTATION__)
INLINE bool isPrintInfo(uint verboseLevel)
{
  return !globalOptions.quietFlag && ((uint)globalOptions.verboseLevel >= verboseLevel);
}
#endif /* NDEBUG || __BAR_IMPLEMENTATION__ */

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
*          width        - width (can be 0)
*          format       - format string (like printf)
*          ...          - optional arguments (like printf)
*          arguments    - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printConsole(FILE *file, uint width, const char *format, ...);

/***********************************************************************\
* Name   : printWarning
* Purpose: output warning on console and write to log file
* Input  : text - format string (like printf)
*          ...  - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printWarning(const char *text, ...);

/***********************************************************************\
* Name   : printError
* Purpose: print error message on stderr and write to log file
*          text - format string (like printf)
*          ...  - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printError(const char *text, ...);

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
* Name   : initLog
* Purpose: init job log
* Input  : logHandle - log handle variable
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors initLog(LogHandle *logHandle);

/***********************************************************************\
* Name   : doneLog
* Purpose: done job log
* Input  : logHandle - log handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneLog(LogHandle *logHandle);

/***********************************************************************\
* Name   : vlogMessage, plogMessage, logMessage
* Purpose: log message into job log and global log
* Input  : logHandle - log handle
*          logType   - log type; see LOG_TYPES_*
*          prefix    - prefix text
*          text      - format string (like printf)
*          arguments - arguments
*          ...       - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void vlogMessage(LogHandle *logHandle, ulong logType, const char *prefix, const char *text, va_list arguments);
void plogMessage(LogHandle *logHandle, ulong logType, const char *prefix, const char *text, ...);
void logMessage(LogHandle *logHandle, ulong logType, const char *text, ...);

/***********************************************************************\
* Name   : logLines
* Purpose: log lines into job log and global log
* Input  : logHandle - log handle
*          logType   - log type; see LOG_TYPES_*
*          prefix    - prefix text
*          lines     - line list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void logLines(LogHandle *logHandle, ulong logType, const char *prefix, const StringList *lines);

/***********************************************************************\
* Name   : fatalLogMessage
* Purpose: log fatal error
* Input  : text     - format string (like printf)
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void fatalLogMessage(const char *text, void *userData);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : getHumanSizeString
* Purpose: get human readable size string
* Input  : buffer     - buffer to format string into
*          bufferSize - size of buffer
*          n          - size value
* Output : -
* Return : buffer with formated human string size
* Notes  : -
\***********************************************************************/

const char* getHumanSizeString(char *buffer, uint bufferSize, uint64 n);

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
* Input  : templateString  - template string
*          timestamp       - timestamp [s] or 0
*          textMacros      - macros array
*          textMacroCount  - number of macros
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors executeTemplate(const char       *templateString,
                       time_t           timestamp,
                       const TextMacro  textMacros[],
                       uint             textMacroCount
                      );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : logPostProcess
* Purpose: log post processing
* Input  : logHandle          - log handle
*          jobOptions         - job options
*          archiveType        - archive type
*          scheduleCustomText - schedule custom text
*          jobName            - job name
*          jobState           - job state
*          storageFlags       - storage flags; see STORAGE_FLAG_...
*          message            - message
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void logPostProcess(LogHandle        *logHandle,
                    const JobOptions *jobOptions,
                    ArchiveTypes     archiveType,
                    ConstString      scheduleCustomText,
                    ConstString      jobName,
                    JobStates        jobState,
                    StorageFlags     storageFlags,
                    ConstString      message
                   );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : executeIOOutput
* Purpose: process exec output
* Input  : line     - line to output and to append to strin glist (if
*                     userData is not NULL)
*          userData - string list or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void executeIOOutput(ConstString line,
                     void        *userData
                    );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : getBandWidth
* Purpose: get band width from value or external file
* Input  : bandWidthList - band width list settings or NULL
* Output : -
* Return : return band width [bits/s] or 0
* Notes  : -
\***********************************************************************/

ulong getBandWidth(BandWidthList *bandWidthList);

// ----------------------------------------------------------------------

#if 0
/***********************************************************************\
* Name   : newMaintenanceNode
* Purpose: create new maintenance node
* Input  : -
* Output : -
* Return : maintenance node
* Notes  : -
\***********************************************************************/

MaintenanceNode *newMaintenanceNode(void);

/***********************************************************************\
* Name   : deleteMaintenanceNode
* Purpose: delete maintenance node
* Input  : maintenanceNode - maintenance node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void deleteMaintenanceNode(MaintenanceNode *maintenanceNode);

/***********************************************************************\
* Name   : freeMaintenanceNode
* Purpose: delete maintenance time node
* Input  : maintenanceNode - maintenance node
*          userData        - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeMaintenanceNode(MaintenanceNode *maintenanceNode, void *userData);

/***********************************************************************\
* Name   : initServer
* Purpose: init server
* Input  : server     - server variable
*          name       - server name
*          serverType - server type
* Output : server - initialized server structure
* Return : -
* Notes  : -
\***********************************************************************/

void initServer(Server *server, ConstString name, ServerTypes serverType);

/***********************************************************************\
* Name   : doneServer
* Purpose: done server
* Input  : server - server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneServer(Server *server);

/***********************************************************************\
* Name   : newServerNode
* Purpose: new server node
* Input  : serverType - server type
*          name       - server name
* Output : -
* Return : server node
* Notes  : -
\***********************************************************************/

ServerNode *newServerNode(ConstString name, ServerTypes serverType);

/***********************************************************************\
* Name   : deleteServerNode
* Purpose: delete server node
* Input  : serverNode - server node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void deleteServerNode(ServerNode *serverNode);

/***********************************************************************\
* Name   : freeServerNode
* Purpose: free server node
* Input  : serverNode - server node
*          userData   - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeServerNode(ServerNode *serverNode, void *userData);
#endif

/***********************************************************************\
* Name   : getServerSettings
* Purpose: get server settings
* Input  : storageName - storage name
*          jobOptions  - job options
* Output : server - server settings from job options, server list
*                   default server values
* Return : server id or 0
* Notes  : -
\***********************************************************************/

uint getServerSettings(Server                 *server,
                       const StorageSpecifier *storageSpecifier,
                       const JobOptions       *jobOptions
                      );

/***********************************************************************\
* Name   : initFileServerSettings
* Purpose: init file server settings
* Input  : directory  - directory
*          jobOptions - job options
* Output : fileServer - file server settings from job options, server
*                       list or default FTP server values
* Return : server id or 0
* Notes  : -
\***********************************************************************/

uint initFileServerSettings(FileServer       *fileServer,
                            ConstString      directory,
                            const JobOptions *jobOptions
                           );

/***********************************************************************\
* Name   : doneFileServerSettings
* Purpose: done file server settings
* Input  : fileServer - file server settings
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneFileServerSettings(FileServer *fileServer);

/***********************************************************************\
* Name   : initFTPServerSettings
* Purpose: init FTP server settings
* Input  : hostName   - FTP server host name
*          jobOptions - job options
* Output : ftpServer - FTP server settings from job options, server
*                      list or default FTP server values
* Return : server id or 0
* Notes  : -
\***********************************************************************/

uint initFTPServerSettings(FTPServer        *ftpServer,
                           ConstString      hostName,
                           const JobOptions *jobOptions
                          );

/***********************************************************************\
* Name   : doneFTPServerSettings
* Purpose: done FTP server settings
* Input  : ftpServer - FTP server settings
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneFTPServerSettings(FTPServer *ftpServer);

/***********************************************************************\
* Name   : initSSHServerSettings
* Purpose: init SSH server settings
* Input  : hostName   - SSH server host name
*          jobOptions - job options
* Output : sshServer  - SSH server settings from job options, server
*                       list or default SSH server values
* Return : server id or 0
* Notes  : -
\***********************************************************************/

uint initSSHServerSettings(SSHServer        *sshServer,
                           ConstString      hostName,
                           const JobOptions *jobOptions
                          );

/***********************************************************************\
* Name   : doneSSHServerSettings
* Purpose: done SSH server settings
* Input  : sshServer - SSH server settings
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneSSHServerSettings(SSHServer *sshServer);

/***********************************************************************\
* Name   : getWebDAVServerSettings
* Purpose: init WebDAV server settings
* Input  : hostName   - WebDAV server host name
*          jobOptions - job options
* Output : webDAVServer - WebDAV server settings from job options,
*                         server list or default WebDAV server values
* Return : server id or 0
* Notes  : -
\***********************************************************************/

uint initWebDAVServerSettings(WebDAVServer     *webDAVServer,
                              ConstString      hostName,
                              const JobOptions *jobOptions
                             );

/***********************************************************************\
* Name   : doneWebDAVServerSettings
* Purpose: done WebDAV server settings
* Input  : webDAVServer - WebDAV server settings
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneWebDAVServerSettings(WebDAVServer *webDAVServer);

/***********************************************************************\
* Name   : getCDSettings
* Purpose: init CD settings
* Input  : jobOptions - job options
* Output : cd - cd settings from job options or default CD values
* Return : -
* Notes  : -
\***********************************************************************/

void initCDSettings(OpticalDisk      *cd,
                    const JobOptions *jobOptions
                   );

/***********************************************************************\
* Name   : getDVDSettings
* Purpose: init DVD settings
* Input  : jobOptions - job options
* Output : dvd - dvd settings from job options or default DVD values
* Return : -
* Notes  : -
\***********************************************************************/

void initDVDSettings(OpticalDisk      *dvd,
                     const JobOptions *jobOptions
                    );

/***********************************************************************\
* Name   : initBDSettings
* Purpose: init BD settings
* Input  : jobOptions - job options
* Output : bd - bd settings from job options or default BD values
* Return : -
* Notes  : -
\***********************************************************************/

void initBDSettings(OpticalDisk      *bd,
                    const JobOptions *jobOptions
                   );

/***********************************************************************\
* Name   : doneOpticalDiskSettings
* Purpose: done optical disk settings
* Input  : opticalDisk - optical disk settings
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneOpticalDiskSettings(OpticalDisk *opticalDisk);

/***********************************************************************\
* Name   : initDeviceSettings
* Purpose: init device settings
* Input  : name       - device name
*          jobOptions - job options
* Output : device - device settings from job options, device list or
*                   default device values
* Return : -
* Notes  : -
\***********************************************************************/

void initDeviceSettings(Device           *device,
                        ConstString      name,
                        const JobOptions *jobOptions
                       );

/***********************************************************************\
* Name   : doneDeviceSettings
* Purpose: done device settings
* Input  : device - device settings
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneDeviceSettings(Device *device);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : allocateServer
* Purpose: allocate server
* Input  : serverId - server id
*          priority - server connection priority; see
*                     SERVER_CONNECTION_PRIORITY_...
*          timeout  - timeout or -1 [ms]
* Output : -
* Return : TRUE iff server allocated, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool allocateServer(uint serverId, ServerConnectionPriorities priority, long timeout);

/***********************************************************************\
* Name   : freeServer
* Purpose: free allocated server
* Input  : serverId - server id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeServer(uint serverId);

/***********************************************************************\
* Name   : isServerAllocationPending
* Purpose: check if a server allocation with high priority is pending
* Input  : serverId - server id
* Output : -
* Return : TRUE if server allocation with high priority is pending,
*          FALSE otherwise
* Notes  : -
\***********************************************************************/

bool isServerAllocationPending(uint serverId);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : mountAll
* Purpose: mount all nodes
* Input  : mountList - mount list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors mountAll(const MountList *mountList);

/***********************************************************************\
* Name   : unmountAll
* Purpose: unmount all nodes
* Input  : mountList - mount list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors unmountAll(const MountList *mountList);

/***********************************************************************\
* Name   : purgeMounts
* Purpose: purge not used mounts
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void purgeMounts(void);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : getCryptPasswordFromConsole
* Purpose: input crypt password via console or external program
* Input  : name          - name variable (not used)
*          password      - password variable
*          passwordType  - password type; see PASSWORD_TYPE_...
*          text          - text
*          validateFlag  - TRUE to validate input, FALSE otherwise
*          weakCheckFlag - TRUE for weak password checking, FALSE
*                          otherwise (print warning if password seems to
*                          be a weak password)
*          userData      - (not used)
* Output : password - crypt password
* Return : ERROR_NONE or error code
* Notes  :
\***********************************************************************/

Errors getCryptPasswordFromConsole(String        name,
                                   Password      *password,
                                   PasswordTypes passwordType,
                                   const char    *text,
                                   bool          validateFlag,
                                   bool          weakCheckFlag,
                                   void          *userData
                                  );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : initFilePattern
* Purpose: init file pattern
* Input  : pattern     - pattern variable
*          string      - pattern
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : pattern - initialized variable
* Return : ERROR_NONE or error code
* Notes  : escape special characters in file name
\***********************************************************************/

Errors initFilePattern(Pattern *pattern, ConstString fileName, PatternTypes patternType);

/***********************************************************************\
* Name   : initStatusInfo
* Purpose: initialize status info
* Input  : statusInfo - status info variable
* Output : statusInfo - initialized create status variable
* Return : -
* Notes  : -
\***********************************************************************/

void initStatusInfo(StatusInfo *statusInfo);

/***********************************************************************\
* Name   : doneStatusInfo
* Purpose: done status info
* Input  : statusInfo - status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneStatusInfo(StatusInfo *statusInfo);

/***********************************************************************\
* Name   : setStatusInfo
* Purpose: set status info from other info
* Input  : statusInfo     - status info
*          fromStatusInfo - from status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void setStatusInfo(StatusInfo *statusInfo, const StatusInfo *fromStatusInfo);

/***********************************************************************\
* Name   : resetStatusInfo
* Purpose: reset status info
* Input  : statusInfo - status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void resetStatusInfo(StatusInfo *statusInfo);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : addStorageNameListFromFile
* Purpose: add content list from file to storage name list
* Input  : entryType - entry type
*          entryList - entry list
*          fileName  - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors addStorageNameListFromFile(StringList *storageNameList, const char *fileName);

/***********************************************************************\
* Name   : addStorageNameListFromCommand
* Purpose: add output of command to storage name list
* Input  : entryType - entry type
*          entryList - entry list
*          fileName  - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors addStorageNameListFromCommand(StringList *storageNameList, const char *template);

/***********************************************************************\
* Name   : addIncludeListFromFile
* Purpose: add content list from file to include entry list
* Input  : entryType - entry type
*          entryList - entry list
*          fileName  - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors addIncludeListFromFile(EntryTypes entryType, EntryList *entryList, const char *fileName);

/***********************************************************************\
* Name   : addIncludeListFromCommand
* Purpose: add output of command to include entry list
* Input  : entryType       - entry type
*          entryList       - entry list
*          commandTemplate - command/script template
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors addIncludeListFromCommand(EntryTypes entryType, EntryList *entryList, const char *commandTemplate);

/***********************************************************************\
* Name   : addExcludeListFromFile
* Purpose: add content list from file to exclude pattern list
* Input  : patternList - pattern list
*          fileName    - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors addExcludeListFromFile(PatternList *patternList, const char *fileName);

/***********************************************************************\
* Name   : addExcludeListFromCommand
* Purpose: add output of command to exclude pattern list
* Input  : patternList     - pattern list
*          commandTemplate - command/script template
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors addExcludeListFromCommand(PatternList *patternList, const char *commandTemplate);

/***********************************************************************\
* Name   : isIncluded
* Purpose: check if name is included
* Input  : includeEntryNode - include entry node
*          name             - name
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool isIncluded(const EntryNode *includeEntryNode,
                ConstString     name
               );

/***********************************************************************\
* Name   : isInIncludedList
* Purpose: check if name is in included list
* Input  : includeEntryList - include entry list
*          name             - name
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool isInIncludedList(const EntryList *includeEntryList,
                      ConstString     name
                     );

/***********************************************************************\
* Name   : isInExcludedList
* Purpose: check if name is in excluded list
* Input  : excludePatternList - exclude pattern list
*          name               - name
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool isInExcludedList(const PatternList *excludePatternList,
                      ConstString       name
                     );

/***********************************************************************\
* Name   : hasNoBackup
* Purpose: check if file .nobackup/.NOBACKUP exists in sub-directory
* Input  : pathName - path name
* Output : -
* Return : TRUE if .nobackup/.NOBACKUP exists and option
*          ignoreNoBackupFile is not set, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool hasNoBackup(ConstString pathName);

/***********************************************************************\
* Name   : hasNoDumpAttribute
* Purpose: check if file has attribute 'no dump'
* Input  : name - file name
* Output : -
* Return : TRUE iff file has 'no dump' attribute
* Notes  : -
\***********************************************************************/

bool hasNoDumpAttribute(ConstString name);

#ifdef __cplusplus
  }
#endif

#endif /* __BAR__ */

/* end of file */
