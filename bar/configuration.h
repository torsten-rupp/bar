/***********************************************************************\
*
* Contents: Backup ARchiver configuration
* Systems: all
*
\***********************************************************************/

#ifndef __CONFIGURATION__
#define __CONFIGURATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <locale.h>
#include <assert.h>

#include "common/global.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/cmdoptions.h"
#include "common/configvalues.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/passwords.h"
#include "common/misc.h"

#include "entrylists.h"
#include "compress.h"
#include "crypt.h"
#include "bar_common.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

extern CommandLineOption COMMAND_LINE_OPTIONS[];

// config values
extern const ConfigValueUnit   CONFIG_VALUE_BYTES_UNITS[];
extern const ConfigValueUnit   CONFIG_VALUE_BITS_UNITS[];
extern const ConfigValueSelect CONFIG_VALUE_TLS_MODES[];
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
extern const ConfigValue       CONFIG_VALUES[];

extern const ConfigValue       JOB_CONFIG_VALUES[];

/***************************** Datatypes *******************************/

// config file type
typedef enum
{
  CONFIG_FILE_TYPE_AUTO,
  CONFIG_FILE_TYPE_COMMAND_LINE,
  CONFIG_FILE_TYPE_CONFIG
} ConfigFileTypes;

// config list
typedef struct ConfigFileNode
{
  LIST_NODE_HEADER(struct ConfigFileNode);

  ConfigFileTypes type;
  String          fileName;
} ConfigFileNode;

typedef struct
{
  LIST_HEADER(ConfigFileNode);
} ConfigFileList;

/***************************** Variables *******************************/
extern GlobalOptions globalOptions;            // global options
extern String        instanceUUID;             // BAR instance UUID

/****************************** Macros *********************************/
#ifndef NDEBUG
  #define Configuration_initKey(...)      __Configuration_initKey     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Configuration_duplicateKey(...) __Configuration_duplicateKey(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Configuration_doneKey(...)      __Configuration_doneKey     (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Configuration_initAll
* Purpose: initialize configuration functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Configuration_initAll(void);

/***********************************************************************\
* Name   : Configuration_doneAll
* Purpose: deinitialize configuration functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneAll(void);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Configuration_isCertificateAvailable
* Purpose: check if certificate is available
* Input  : certificate - certificate
* Output : -
* Return : TRUE iff certificate is available
* Notes  : -
\***********************************************************************/

INLINE bool Configuration_isCertificateAvailable(const Certificate *certificate);
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE bool Configuration_isCertificateAvailable(const Certificate *certificate)
{
  assert(certificate != NULL);

  return (certificate->data != NULL) && (certificate->length > 0);
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Configuration_initKey
* Purpose: init empty public/private key
* Input  : key - key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Configuration_initKey(Key *key);
#else /* not NDEBUG */
void __Configuration_initKey(const char *__fileName__,
                             ulong      __lineNb__,
                             Key        *key
                            );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Configuration_duplicateKey
* Purpose: duplicate public/private key
* Input  : key     - key variable
*          fromKey - from key
* Output : -
* Return : TRUE iff key duplicated
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool Configuration_duplicateKey(Key *key, const Key *fromKey);
#else /* not NDEBUG */
bool __Configuration_duplicateKey(const char *__fileName__,
                                  ulong      __lineNb__,
                                  Key        *key,
                                  const Key  *fromKey
                                 );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Configuration_doneKey
* Purpose: done public/private key
* Input  : key - key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Configuration_doneKey(Key *key);
#else /* not NDEBUG */
void __Configuration_doneKey(const char *__fileName__,
                             ulong      __lineNb__,
                             Key        *key
                            );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Configuration_setKey
* Purpose: set public/private key
* Input  : key      - key variable
*          fileName - file name or NULL
*          data     - key data
*          length   - length of key data [bytes]
* Output : key - key
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool Configuration_setKey(Key *key, const char *fileName, const void *data, uint length);

/***********************************************************************\
* Name   : Configuration_setKeyString
* Purpose: set public/private key with string
* Input  : key      - key
*          fileName - file name or NULL
*          string   - key data (PEM encoded)
* Output : key - key
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool Configuration_setKeyString(Key *key, const char *fileName, ConstString string);

/***********************************************************************\
* Name   : Configuration_clearKey
* Purpose: clear public/private key
* Input  : key - key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_clearKey(Key *key);

/***********************************************************************\
* Name   : Configuration_copyKey
* Purpose: copy public/private key
* Input  : key     - key variable
*          fromKey - from key
* Output : -
* Return : TRUE iff key copied
* Notes  : -
\***********************************************************************/

bool Configuration_copyKey(Key *key, const Key *fromKey);

/***********************************************************************\
* Name   : Configuration_isKeyAvailable
* Purpose: check if public/private key is available
* Input  : key - key
* Output : -
* Return : TRUE iff key is available
* Notes  : -
\***********************************************************************/

INLINE bool Configuration_isKeyAvailable(const Key *key);
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE bool Configuration_isKeyAvailable(const Key *key)
{
  assert(key != NULL);

  return (key->data != NULL) && (key->length > 0);
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Configuration_keyEquals
* Purpose: check if public/private key equals
* Input  : key0,key1 - keys
* Output : -
* Return : TRUE iff keys equals
* Notes  : -
\***********************************************************************/

INLINE bool Configuration_keyEquals(const Key *key0, const Key *key1);
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE bool Configuration_keyEquals(const Key *key0, const Key *key1)
{
  if      ((key0 != NULL) && (key1 != NULL))
  {
    return memEquals(key0->data,key0->length,key1->data,key1->length);
  }
  else if ((key0 == NULL) && (key1 == NULL))
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Configuration_setHash
* Purpose: set hash
* Input  : hash      - hash
*          cryptHash - crypt hash
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool Configuration_setHash(Hash *hash, const CryptHash *cryptHash);

/***********************************************************************\
* Name   : Configuration_clearHash
* Purpose: clear hash
* Input  : hash - hash
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_clearHash(Hash *hash);

/***********************************************************************\
* Name   : Configuration_isHashAvailable
* Purpose: check if hash is available
* Input  : hash - hash
* Output : -
* Return : TRUE iff hash is available
* Notes  : -
\***********************************************************************/

INLINE bool Configuration_isHashAvailable(const Hash *hash);
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE bool Configuration_isHashAvailable(const Hash *hash)
{
  assert(hash != NULL);

  return (hash->data != NULL) && (hash->length > 0);
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Configuration_equalsHash
* Purpose: check if hash equals crypt hash
* Input  : hash      - hash
*          cryptHash - crypt hash
* Output : -
* Return : TRUE iff hashes equals
* Notes  : -
\***********************************************************************/

bool Configuration_equalsHash(Hash *hash, const CryptHash *cryptHash);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Configuration_parseWeekDaySet
* Purpose: parse date week day set
* Input  : names - day names to parse
* Output : weekDaySet - week day set
* Return : TRUE iff week day parsed
* Notes  : -
\***********************************************************************/

bool Configuration_parseWeekDaySet(const char *names, WeekDaySet *weekDaySet);

/***********************************************************************\
* Name   : Configuration_parseDateNumber
* Purpose: parse date/time number (year, day, month)
* Input  : s - string to parse
* Output : n - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

bool Configuration_parseDateNumber(ConstString s, int *n);

/***********************************************************************\
* Name   : Configuration_parseDateMonth
* Purpose: parse date month name
* Input  : s - string to parse
* Output : month - month (MONTH_JAN..MONTH_DEC)
* Return : TRUE iff month parsed
* Notes  : -
\***********************************************************************/

bool Configuration_parseDateMonth(ConstString s, int *month);

/***********************************************************************\
* Name   : Configuration_parseTimeNumber
* Purpose: parse time number (hour, minute)
* Input  : s - string to parse
* Output : n - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

bool Configuration_parseTimeNumber(ConstString s, int *n);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Configuration_newMaintenanceNode
* Purpose: new maintenance node
* Input  : -
* Output : -
* Return : maintenance node
* Notes  : -
\***********************************************************************/

MaintenanceNode *Configuration_newMaintenanceNode(void);

/***********************************************************************\
* Name   : Configuration_deleteMaintenanceNode
* Purpose: delete maintenance node
* Input  : maintenanceNode - maintenance node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_deleteMaintenanceNode(MaintenanceNode *maintenanceNode);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Configuration_initServer
* Purpose: init server
* Input  : server     - server variable
*          name       - server name
*          serverType - server type
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_initServer(Server *server, ConstString name, ServerTypes serverType);

/***********************************************************************\
* Name   : Configuration_doneServer
* Purpose: done server
* Input  : server - server variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneServer(Server *server);

/***********************************************************************\
* Name   : Configuration_initFileServerSettings
* Purpose: init device settings
* Input  : fileServer - files server variable
*          directory  - directory
*          jobOptions - job options
* Output : fileServer - files server
* Return : -
* Notes  : -
\***********************************************************************/

uint Configuration_initFileServerSettings(FileServer       *fileServer,
                                          ConstString      directory,
                                          const JobOptions *jobOptions
                                         );

/***********************************************************************\
* Name   : Configuration_doneFileServerSettings
* Purpose: done file server settings
* Input  : fileServer - file ferver
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneFileServerSettings(FileServer *fileServer);

/***********************************************************************\
* Name   : Configuration_initFTPServerSettings
* Purpose: init device settings
* Input  : ftpServer  - FTP server variable
*          hostName   - host name
*          jobOptions - job options
* Output : ftpServer - FTP server
* Return : -
* Notes  : -
\***********************************************************************/

uint Configuration_initFTPServerSettings(FTPServer        *ftpServer,
                                         ConstString      hostName,
                                         const JobOptions *jobOptions
                                        );

/***********************************************************************\
* Name   : Configuration_doneFTPServerSettings
* Purpose: done ftp server settings
* Input  : ftpServer - FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneFTPServerSettings(FTPServer *ftpServer);

/***********************************************************************\
* Name   : Configuration_initSSHServerSettings
* Purpose: init SSH server settings
* Input  : sshServer  - SSH server variable
*          hostName   - host name
*          jobOptions - job options
* Output : sshServer - SSH server
* Return : -
* Notes  : -
\***********************************************************************/

uint Configuration_initSSHServerSettings(SSHServer        *sshServer,
                                         ConstString      hostName,
                                         const JobOptions *jobOptions
                                        );

/***********************************************************************\
* Name   : Configuration_doneSSHServerSettings
* Purpose: done SSH server settings
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneSSHServerSettings(SSHServer *sshServer);

/***********************************************************************\
* Name   : Configuration_initWebDAVServerSettings
* Purpose: init webDAV server settings
* Input  : webDAVServer - webDAV server variable
*          hostName     - host name
*          jobOptions   - job options
* Output : webDAVServer - webDAV server
* Return : -
* Notes  : -
\***********************************************************************/

uint Configuration_initWebDAVServerSettings(WebDAVServer     *webDAVServer,
                                            ConstString      hostName,
                                            const JobOptions *jobOptions
                                           );


/***********************************************************************\
* Name   : Configuration_doneWebDAVServerSettings
* Purpose: done webDAV server settings
* Input  : webDAVServer - webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneWebDAVServerSettings(WebDAVServer *webDAVServer);

/***********************************************************************\
* Name   : Configuration_initWebDAVSServerSettings
* Purpose: init webDAVs server settings
* Input  : webDAVServer - webDAV server
*          hostName     - host name
*          jobOptions   - job options
* Output : webDAVServer - webDAV server
* Return : -
* Notes  : -
\***********************************************************************/

uint Configuration_initWebDAVSServerSettings(WebDAVServer     *webDAVServer,
                                             ConstString      hostName,
                                             const JobOptions *jobOptions
                                            );


/***********************************************************************\
* Name   : Configuration_doneWebDAVSServerSettings
* Purpose: done webDAVS server settings
* Input  : webDAVServer - webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneWebDAVSServerSettings(WebDAVServer *webDAVServer);

/***********************************************************************\
* Name   : Configuration_initSMBServerSettings
* Purpose: init SMB/CIFS settings
* Input  : smbServer  - SMB server variable
*          hostName   - host name
*          jobOptions - job options
* Output : smbServer - SMB server
* Return : -
* Notes  : -
\***********************************************************************/

uint Configuration_initSMBServerSettings(SMBServer        *smbServer,
                                         ConstString      hostName,
                                         const JobOptions *jobOptions
                                        );

/***********************************************************************\
* Name   : Configuration_doneSMBServerSettings
* Purpose: done SMB/CIFS settings
* Input  : smbServer - SMB server variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneSMBServerSettings(SMBServer *smbServer);

/***********************************************************************\
* Name   : Configuration_newServerNode
* Purpose: new config file node
* Input  : name       - sever name
*          serverType - server type
* Output : -
* Return : server node
* Notes  : -
\***********************************************************************/

ServerNode *Configuration_newServerNode(ConstString name, ServerTypes serverType);

/***********************************************************************\
* Name   : Configuration_deleteServerNode
* Purpose: delete server mode
* Input  : serverNode - server node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_deleteServerNode(ServerNode *serverNode);

/***********************************************************************\
* Name   : Configuration_setServerNode
* Purpose: set server node
* Input  : serverNode - server node
*          name       - sever name
*          serverType - server type
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_setServerNode(ServerNode *serverNode, ConstString name, ServerTypes serverType);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Configuration_initCDSettings
* Purpose: init device settings
* Input  : cd         - cd variable
*          jobOptions - job options
* Output : cd - cd
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_initCDSettings(OpticalDisk      *cd,
                                  const JobOptions *jobOptions
                                 );

/***********************************************************************\
* Name   : Configuration_initDVDSettings
* Purpose: init device settings
* Input  : dvd        - dvd variable
*          jobOptions - job options
* Output : dvd - dvd
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_initDVDSettings(OpticalDisk      *dvd,
                                   const JobOptions *jobOptions
                                  );

/***********************************************************************\
* Name   : Configuration_initBDSettings
* Purpose: init device settings
* Input  : bd         - bd variable
*          jobOptions - job options
* Output : bd - bd
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_initBDSettings(OpticalDisk      *bd,
                                  const JobOptions *jobOptions
                                 );

/***********************************************************************\
* Name   : Configuration_doneOpticalDiskSettings
* Purpose: done opticaldisk settings
* Input  : opticalDisk - optical disk
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneOpticalDiskSettings(OpticalDisk *opticalDisk);

/***********************************************************************\
* Name   : Configuration_initDeviceSettings
* Purpose: init device settings
* Input  : device     - device variable
*          name       - name
*          jobOptions - job options
* Output : device - device
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_initDeviceSettings(Device           *device,
                                      ConstString      name,
                                      const JobOptions *jobOptions
                                     );

/***********************************************************************\
* Name   : Configuration_doneDeviceSettings
* Purpose: done device settings
* Input  : device - device
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneDeviceSettings(Device *device);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Configuration_newDeviceNode
* Purpose: new server node
* Input  : name - device name
* Output : -
* Return : device node
* Notes  : -
\***********************************************************************/

DeviceNode *Configuration_newDeviceNode(ConstString name);

/***********************************************************************\
* Name   : Configuration_freeDeviceNode
* Purpose: free device node
* Input  : deviceNode - device node
*          userData   - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_freeDeviceNode(DeviceNode *deviceNode, void *userData);

/***********************************************************************\
* Name   : Configuration_deleteDeviceNode
* Purpose: delete device node
* Input  : deviceNode - device node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_deleteDeviceNode(DeviceNode *deviceNode);

/***********************************************************************\
* Name   : Configuration_newMountNode
* Purpose: new mount node
* Input  : mountName  - mount name
*          deviceName - device name
* Output : -
* Return : mount node
* Notes  : -
\***********************************************************************/

MountNode *Configuration_newMountNode(ConstString mountName, ConstString deviceName);

/***********************************************************************\
* Name   : Configuration_duplicateMountNode
* Purpose: duplicate mount node
* Input  : fromMountNode - from mount name
*          userData      - user data
* Output : -
* Return : mount node
* Notes  : -
\***********************************************************************/

MountNode *Configuration_duplicateMountNode(MountNode *fromMountNode,
                                            void      *userData
                                           );

/***********************************************************************\
* Name   : Configuration_freeMountNode
* Purpose: free mount node
* Input  : mountNode - mount node
*          userData  - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_freeMountNode(MountNode *mountNode, void *userData);

/***********************************************************************\
* Name   : Configuration_deleteMountNode
* Purpose: delete mount node
* Input  : mountNode - mount node
*          userData  - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_deleteMountNode(MountNode *mountNode);

/***********************************************************************\
* Name   : Configuration_freeBandWidthNode
* Purpose: free band width node
* Input  : bandWidthNode - band width node
*          userData      - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_freeBandWidthNode(BandWidthNode *bandWidthNode, void *userData);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Configuration_add
* Purpose: add configuration file
* Input  : configFileType - config file type
*          fileName       - file name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_add(ConfigFileTypes configFileType, const char *fileName);

/***********************************************************************\
* Name   : Configuration_setModified
* Purpose: set configuration modified
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Configuration_setModified(void);
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE void Configuration_setModified(void)
{
  extern bool configModified;

  configModified = TRUE;
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Configuration_clearModified
* Purpose: clear configuration modified
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Configuration_clearModified(void);
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE void Configuration_clearModified(void)
{
  extern bool configModified;

  configModified = FALSE;
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Configuration_isModified
* Purpose: check if configuration modified
* Input  : -
* Output : -
* Return : TRUE iff modified
* Notes  : -
\***********************************************************************/

INLINE bool Configuration_isModified(void);
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE bool Configuration_isModified(void)
{
  extern bool configModified;

  return configModified;
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Configuration_readAll
* Purpose: read all configuration from files
* Input  : printInfoFlag - TRUE for output info, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Configuration_readAll(bool printInfoFlag);

/***********************************************************************\
* Name   : Configuration_readAllServerKeysCertificates
* Purpose: initialize all server keys/certificates
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Configuration_readAllServerKeysCertificates(void);

/***********************************************************************\
* Name   : Configuration_validate
* Purpose: validate options
* Input  : -
* Output : -
* Return : TRUE if options valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Configuration_validate(void);

/***********************************************************************\
* Name   : updateConfig
* Purpose: update configuration file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Configuration_update(void);

#ifdef __cplusplus
  }
#endif

#endif /* __CONFIGURATION__ */

/* end of file */
