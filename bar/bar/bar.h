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

#include "common/global.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/configvalues.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/passwords.h"
#include "common/misc.h"

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

// config values
extern const ConfigValueUnit   CONFIG_VALUE_BYTES_UNITS[];
extern const ConfigValueUnit   CONFIG_VALUE_BITS_UNITS[];
extern const ConfigValueSelect CONFIG_VALUE_ARCHIVE_TYPES[];
extern const ConfigValueSelect CONFIG_VALUE_PATTERN_TYPES[];
extern const ConfigValueSelect CONFIG_VALUE_COMPRESS_ALGORITHMS[];
extern const ConfigValueSelect CONFIG_VALUE_CRYPT_ALGORITHMS[];
extern const ConfigValueSelect CONFIG_VALUE_CRYPT_TYPES[];
extern const ConfigValueSelect CONFIG_VALUE_PASSWORD_MODES[];
extern const ConfigValueUnit   CONFIG_VALUE_TIME_UNITS[];
extern const ConfigValueSet    CONFIG_VALUE_LOG_TYPES[];
extern const ConfigValueSelect CONFIG_VALUE_ARCHIVE_FILE_MODES[];
extern const ConfigValueSelect CONFIG_VALUE_RESTORE_ENTRY_MODES[];
extern ConfigValue             CONFIG_VALUES[];

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
extern GlobalOptions   globalOptions;          // global options
extern GlobalOptionSet globalOptionSet;        // global option set
extern String          uuid;                   // BAR instance UUID
extern String          tmpDirectory;           // temporary directory
extern Semaphore       consoleLock;            // lock console
extern locale_t        POSIXLocale;            // POSIX locale

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

/***********************************************************************\
* Name   : initCertificate
* Purpose: init empty certificate
* Input  : certificate - certificate
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void initCertificate(Certificate *certificate);

/***********************************************************************\
* Name   : duplicateCertificate
* Purpose: duplicate certificate
* Input  : toCertificate   - certificate variable
*          fromCertificate - from certificate
* Output : -
* Return : TRUE iff certificate duplicated
* Notes  : -
\***********************************************************************/

bool duplicateCertificate(Certificate *toCertificate, const Certificate *fromCertificate);

/***********************************************************************\
* Name   : doneCertificate
* Purpose: free certificate
* Input  : certificate - certificate
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneCertificate(Certificate *certificate);

/***********************************************************************\
* Name   : isCertificateAvailable
* Purpose: check if certificate is available
* Input  : certificate - certificate
* Output : -
* Return : TRUE iff certificate is available
* Notes  : -
\***********************************************************************/

bool isCertificateAvailable(const Certificate *certificate);

/***********************************************************************\
* Name   : clearCertificate
* Purpose: clear certificate
* Input  : certificate - certificate
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void clearCertificate(Certificate *certificate);

/***********************************************************************\
* Name   : setCertificate
* Purpose: set certificate
* Input  : certificate       - certificate
*          certificateData   - certificate data
*          certificateLength - length of certificate data [bytes]
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool setCertificate(Certificate *certificate, const void *certificateData, uint certificateLength);

/***********************************************************************\
* Name   : setCertificateString
* Purpose: set certificate with string
* Input  : certificate - certificate
*          string      - certificate data
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool setCertificateString(Certificate *kecertificatey, ConstString string);

//TODO: remove
#if 0
/***********************************************************************\
* Name   : readCAFile
* Purpose: read certicate authority file
* Input  : certificate - certificate variable
*          fileName    - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors readCAFile(Certificate *certificate, const char *fileName);
#endif

/***********************************************************************\
* Name   : initKey
* Purpose: init empty public/private key
* Input  : key - key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void initKey(Key *key);

/***********************************************************************\
* Name   : setKey
* Purpose: set public/private key
* Input  : key    - key
*          data   - key data
*          length - length of key data [bytes]
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool setKey(Key *key, const void *data, uint length);

/***********************************************************************\
* Name   : setKeyString
* Purpose: set public/private key with string
* Input  : key    - key
*          string - key data (PEM encoded)
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool setKeyString(Key *key, ConstString string);

/***********************************************************************\
* Name   : duplicateKey
* Purpose: duplicate public/private key
* Input  : key     - key variable
*          fromKey - from key
* Output : -
* Return : TRUE iff key duplicated
* Notes  : -
\***********************************************************************/

bool duplicateKey(Key *key, const Key *fromKey);

/***********************************************************************\
* Name   : doneKey
* Purpose: free public/private key
* Input  : key - key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneKey(Key *key);

/***********************************************************************\
* Name   : clearKey
* Purpose: clear public/private key
* Input  : key - key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void clearKey(Key *key);

/***********************************************************************\
* Name   : copyKey
* Purpose: copy public/private key
* Input  : key     - key variable
*          fromKey - from key
* Output : -
* Return : TRUE iff key copied
* Notes  : -
\***********************************************************************/

bool copyKey(Key *key, const Key *fromKey);

/***********************************************************************\
* Name   : isKeyAvailable
* Purpose: check if public/private key is available
* Input  : key - key
* Output : -
* Return : TRUE iff key is available
* Notes  : -
\***********************************************************************/

INLINE bool isKeyAvailable(const Key *key);
#if defined(NDEBUG) || defined(__BAR_IMPLEMENTATION__)
INLINE bool isKeyAvailable(const Key *key)
{
  assert(key != NULL);

  return key->data != NULL;
}
#endif /* NDEBUG || __BAR_IMPLEMENTATION__ */

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : initHash
* Purpose: init hash
* Input  : hash - hash variable
* Output : hash - empty hash
* Return : -
* Notes  : -
\***********************************************************************/

void initHash(Hash *hash);

/***********************************************************************\
* Name   : setHash
* Purpose: set hash
* Input  : hash      - hash
*          cryptHash - crypt hash
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool setHash(Hash *hash, const CryptHash *cryptHash);

/***********************************************************************\
* Name   : doneHash
* Purpose: done hash
* Input  : hash - hash
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneHash(Hash *hash);

/***********************************************************************\
* Name   : clearHash
* Purpose: clear hash
* Input  : hash - hash
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void clearHash(Hash *hash);

/***********************************************************************\
* Name   : isHashAvailable
* Purpose: check if hash is available
* Input  : hash - hash
* Output : -
* Return : TRUE iff hash is available
* Notes  : -
\***********************************************************************/

INLINE bool isHashAvailable(const Hash *hash);
#if defined(NDEBUG) || defined(__BAR_IMPLEMENTATION__)
INLINE bool isHashAvailable(const Hash *hash)
{
  assert(hash != NULL);

  return hash->data != NULL;
}
#endif /* NDEBUG || __BAR_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : equalsHash
* Purpose: clear hash
* Input  : hash      - hash
*          cryptHash - crypt hash
* Output : -
* Return : TRUE if hashes equals
* Notes  : -
\***********************************************************************/

bool equalsHash(Hash *hash, const CryptHash *cryptHash);

// ----------------------------------------------------------------------

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
* Name   : newMountNode, newMountNodeCString
* Purpose: new mount node
* Input  : mountName  - mount name
*          deviceName - device name (can be NULL)
* Output : -
* Return : mount node
* Notes  : -
\***********************************************************************/

MountNode *newMountNode(ConstString mountName, ConstString deviceName);
MountNode *newMountNodeCString(const char *mountName, const char *deviceName);

/***********************************************************************\
* Name   : duplicateMountNode
* Purpose: duplicate schedule node
* Input  : fromMountNode - from mount node
*          userData      - user data (not used)
* Output : -
* Return : duplicated mount node
* Notes  : -
\***********************************************************************/

MountNode *duplicateMountNode(MountNode *fromMountNode,
                              void      *userData
                             );

/***********************************************************************\
* Name   : deleteMountNode
* Purpose: delete mount node
* Input  : mountNode - mount node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void deleteMountNode(MountNode *mountNode);

/***********************************************************************\
* Name   : freeMountNode
* Purpose: free mount node
* Input  : mountNode - mount node
*          userData  - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeMountNode(MountNode *mountNode, void *userData);

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

bool parseDateTimeNumber(ConstString s, int *n);

/***********************************************************************\
* Name   : parseDateMonth
* Purpose: parse date month name
* Input  : s - string to parse
* Output : month - month (MONTH_JAN..MONTH_DEC)
* Return : TRUE iff month parsed
* Notes  : -
\***********************************************************************/

bool parseDateMonth(ConstString s, int *month);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : configValueParsePassword
* Purpose: config value option call back for parsing password
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored into variable, FALSE
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
* Name   : configValueFormatCryptAlgorithms
* Purpose: format crypt algorithms config statement
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
* Name   : configValueParseCryptAlgorithms
* Purpose: config value option call back for parsing crypt algorithms
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseCryptAlgorithms(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatInitCryptAlgorithms
* Purpose: init format config crypt algorithms
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitCryptAlgorithms(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneCryptAlgorithms
* Purpose: done format of config crypt algorithms setting
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneCryptAlgorithms(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatCryptAlgorithms
* Purpose: format crypt algorithms config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatCryptAlgorithms(void **formatUserData, void *userData, String line);

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
* Return : TRUE if config value parsed and stored into variable, FALSE
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
* Purpose: config value call back for parsing owner patterns
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored into variable, FALSE
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
* Name   : configValueParsePermissions
* Purpose: config value call back for parsing permissions
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParsePermissions(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatInitPermissions
* Purpose: init format of config owner statements
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitPermissions(void **formatUserData, void *userData, void *variable);

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

void configValueFormatDonePermissions(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatPermissions
* Purpose: format next config owner statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatPermissions(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseFileEntryPattern,
*          configValueParseImageEntryPattern
* Purpose: config value option call back for parsing file/image entry
*          patterns
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseFileEntryPattern(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);
bool configValueParseImageEntryPattern(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatInitEntryPattern
* Purpose: init format of config include statements
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitEntryPattern(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneEntryPattern
* Purpose: done format of config include statements
* Input  : formatUserData - format user data
*          userData       - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneEntryPattern(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatFileEntryPattern,
*          configValueFormatImageEntryPattern
* Purpose: format next config include statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatFileEntryPattern(void **formatUserData, void *userData, String line);
bool configValueFormatImageEntryPattern(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParsePattern
* Purpose: config value option call back for parsing pattern
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored into variable, FALSE
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
* Name   : configValueParseMount
* Purpose: config value option call back for parsing mounts
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseMount(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueFormatInitMount
* Purpose: init format of config mount statements
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitMount(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneMount
* Purpose: done format of config mount statements
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneMount(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatMount
* Purpose: format next config mount statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatMount(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseDeltaSource
* Purpose: config value option call back for parsing delta source
*          pattern
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseDeltaSource(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatInitDeltaSource
* Purpose: init format of config delta source pattern statements
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitDeltaSource(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneDeltaSource
* Purpose: done format of config delta source pattern statements
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneDeltaSource(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatDeltaSource
* Purpose: format next config delta source pattern statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatDeltaSource(void **formatUserData, void *userData, String line);

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
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseString(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueParseDeltaCompressAlgorithm
* Purpose: config value option call back for parsing delta compress
*          algorithm
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseDeltaCompressAlgorithm(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitDeltaCompressAlgorithm
* Purpose: init format config compress algorithms
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
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseByteCompressAlgorithm(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitByteCompressAlgorithms
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
* Name   : configValueParseCompressAlgorithms
* Purpose: config value option call back for parsing compress algorithm
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseCompressAlgorithms(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueFormatInitCompressAlgorithms
* Purpose: init format config compress algorithm
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitCompressAlgorithms(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneCompressAlgorithms
* Purpose: done format of config compress algorithm
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneCompressAlgorithms(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatCompressAlgorithms
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

bool configValueFormatCompressAlgorithms(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseCrypyAlgorithms
* Purpose: config value option call back for parsing crypt algorithm
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseCrypyAlgorithms(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueFormatInitCrypyAlgorithms
* Purpose: init format config crypt algorithm
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitCrypyAlgorithms(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneCrypyAlgorithms
* Purpose: done format of config crypt algorithm
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneCrypyAlgorithms(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatCrypyAlgorithms
* Purpose: format crypt algorithm config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatCrypyAlgorithms(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseCertificate
* Purpose: config value option call back for parsing certificate
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : read from file or decode base64 data
\***********************************************************************/

bool configValueParseCertificate(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueParseKeyData
* Purpose: config value option call back for parsing key data
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : read from file or decode base64 data
\***********************************************************************/

bool configValueParseKeyData(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueFormatInitKeyData
* Purpose: init format config key data
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitKeyData(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneKeyData
* Purpose: done format of config key data
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneKeyData(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatKeyData
* Purpose: format key data config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatKeyData(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseHashData
* Purpose: config value option call back for parsing hash data
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : read from file or decode base64 data
\***********************************************************************/

bool configValueParseHashData(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueFormatInitHashData
* Purpose: init format config hash data
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitHashData(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneHashData
* Purpose: done format of config hash data
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneHashData(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatHashData
* Purpose: format hash data config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatHashData(void **formatUserData, void *userData, String line);

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
* Name   : isNoBackup
* Purpose: check if file .nobackup/.NOBACKUP exists in sub-directory
* Input  : pathName - path name
* Output : -
* Return : TRUE if .nobackup/.NOBACKUP exists and option
*          ignoreNoBackupFile is not set, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool isNoBackup(ConstString pathName);

/***********************************************************************\
* Name   : isNoDumpAttribute
* Purpose: check if file attribute 'no dump' is set
* Input  : fileInfo   - file info
*          jobOptions - job options
* Output : -
* Return : TRUE if 'no dump' attribute is set and option ignoreNoDump is
*          not set, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool isNoDumpAttribute(const FileInfo *fileInfo, const JobOptions *jobOptions);
#if defined(NDEBUG) || defined(__BAR_IMPLEMENTATION__)
INLINE bool isNoDumpAttribute(const FileInfo *fileInfo, const JobOptions *jobOptions)
{
  assert(fileInfo != NULL);
  assert(jobOptions != NULL);

  return !jobOptions->ignoreNoDumpAttributeFlag && File_haveAttributeNoDump(fileInfo);
}
#endif /* NDEBUG || __BAR_IMPLEMENTATION__ */

#ifdef __cplusplus
  }
#endif

#endif /* __BAR__ */

/* end of file */
