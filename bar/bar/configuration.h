/***********************************************************************\
*
* $Revision: 11180 $
* $Date: 2020-11-19 12:31:04 +0100 (Thu, 19 Nov 2020) $
* $Author: torsten $
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

extern CommandLineOption COMMAND_LINE_OPTIONS[];
extern uint              COMMAND_LINE_OPTIONS_COUNT;

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

// mounted list
typedef struct MountedNode
{
  LIST_NODE_HEADER(struct MountedNode);

  String name;                                                // mount point
  String device;                                              // mount device (optional)
  uint   mountCount;                                          // mount count (unmount iff 0)
  uint64 lastMountTimestamp;
} MountedNode;

typedef struct
{
  LIST_HEADER(MountedNode);

  Semaphore lock;
} MountedList;

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

/***************************** Variables *******************************/
extern GlobalOptions   globalOptions;          // global options
extern GlobalOptionSet globalOptionSet;        // global option set
extern String          uuid;                   // BAR instance UUID
extern String          tmpDirectory;           // temporary directory

// Note: initialized once only here
extern bool               daemonFlag;
extern bool               noDetachFlag;
extern bool               versionFlag;
extern bool               helpFlag;
extern bool               xhelpFlag;
extern bool               helpInternalFlag;

extern MountedList        mountedList;                      // list of current mounts

extern Commands           command;
extern String             jobUUIDName;

extern String             jobUUID;                          // UUID of job to execute/convert
extern String             storageName;
extern EntryList          includeEntryList;                 // included entries
extern PatternList        excludePatternList;               // excluded entry patterns

extern StorageFlags       storageFlags;

extern const char         *changeToDirectory;

extern Server             defaultFileServer;
extern Server             defaultFTPServer;
extern Server             defaultSSHServer;
extern Server             defaultWebDAVServer;
extern Device             defaultDevice;
extern ServerModes        serverMode;
extern uint               serverPort;
extern uint               serverTLSPort;
extern Certificate        serverCA;
extern Certificate        serverCert;
extern Key                serverKey;
extern Hash               serverPasswordHash;
extern uint               serverMaxConnections;

extern const char         *continuousDatabaseFileName;
extern const char         *indexDatabaseFileName;

extern ulong              logTypes;
extern const char         *logFileName;
extern const char         *logFormat;
extern const char         *logPostCommand;

extern bool               batchFlag;

extern const char         *pidFileName;

extern uint               generateKeyBits;
extern uint               generateKeyMode;

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

// ----------------------------------------------------------------------

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
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE bool isKeyAvailable(const Key *key)
{
  assert(key != NULL);

  return key->data != NULL;
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : keyEquals
* Purpose: check if public/private key equals
* Input  : key0,key1 - keys
* Output : -
* Return : TRUE iff keys equals
* Notes  : -
\***********************************************************************/

INLINE bool keyEquals(const Key *key0, const Key *key1);
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE bool keyEquals(const Key *key0, const Key *key1)
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
#if defined(NDEBUG) || defined(__CONFIGURATION_IMPLEMENTATION__)
INLINE bool isHashAvailable(const Hash *hash)
{
  assert(hash != NULL);

  return hash->data != NULL;
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

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
* Name   : parseWeekDaySet
* Purpose: parse date week day set
* Input  : names - day names to parse
* Output : weekDaySet - week day set
* Return : TRUE iff week day parsed
* Notes  : -
\***********************************************************************/

//bool parseWeekDaySet(const char *names, WeekDaySet *weekDaySet);

/***********************************************************************\
* Name   : parseDateNumber
* Purpose: parse date/time number (year, day, month)
* Input  : s - string to parse
* Output : n - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

//bool parseDateNumber(ConstString s, int *n);

/***********************************************************************\
* Name   : parseDateMonth
* Purpose: parse date month name
* Input  : s - string to parse
* Output : month - month (MONTH_JAN..MONTH_DEC)
* Return : TRUE iff month parsed
* Notes  : -
\***********************************************************************/

//bool parseDateMonth(ConstString s, int *month);

/***********************************************************************\
* Name   : parseTimeNumber
* Purpose: parse time number (hour, minute)
* Input  : s - string to parse
* Output : n - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

//bool parseTimeNumber(ConstString s, int *n);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : configValuePasswordParse
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

bool configValuePasswordParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatCryptAlgorithms
* Purpose: format crypt algorithms config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValuePasswordFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueCryptAlgorithmsParse
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

bool configValueCryptAlgorithmsParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatCryptAlgorithms
* Purpose: format crypt algorithms config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueCryptAlgorithmsFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueBandWidthParse
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

bool configValueBandWidthParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatBandWidth
* Purpose: format next config band width setting
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueBandWidthFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueOwnerParse
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

bool configValueOwnerParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueOwnerFormat
* Purpose: format next config owner statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueOwnerFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValuePermissionsParse
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

bool configValuePermissionsParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValuePermissionsFormat
* Purpose: format next config owner statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValuePermissionsFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueFileEntryPatternParse
*          configValueImageEntryPatternParse
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

bool configValueFileEntryPatternParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);
bool configValueImageEntryPatternParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatFileEntryPattern,
*          configValueFormatImageEntryPattern
* Purpose: format next config include statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFileEntryPatternFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);
bool configValueImageEntryPatternFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValuePatternParse
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

bool configValuePatternParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatPattern
* Purpose: format next config pattern statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValuePatternFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueMountParse
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

bool configValueMountParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueFormatMount
* Purpose: format next config mount statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueMountFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueDeltaSourceParse
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

bool configValueDeltaSourceParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueFormatDeltaSource
* Purpose: format next config delta source pattern statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueDeltaSourceFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueStringParse
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

bool configValueStringParse(void *userData, void *variable, const char *name, const char *value, char *errorMessage, uint maxErrorMessageLength);

/***********************************************************************\
* Name   : configValueDeltaCompressAlgorithmParse
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

bool configValueDeltaCompressAlgorithmParse(void *userData, void *variable, const char *name, const char *value);

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

void configValueInitDeltaCompressAlgorithmFormat(void **formatUserData, void *userData, void *variable);

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

void configValueDoneDeltaCompressAlgorithmFormat(void **formatUserData, void *userData);

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

bool configValueDeltaCompressAlgorithmFormat(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueByteCompressAlgorithmParse
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

bool configValueByteCompressAlgorithmParse(void *userData, void *variable, const char *name, const char *value);

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

bool configValueByteCompressAlgorithmFormat(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueCompressAlgorithmsParse
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

bool configValueCompressAlgorithmsParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueCompressAlgorithmsFormat
* Purpose: format compress algorithm config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueCompressAlgorithmsFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueCryptAlgorithmsParse
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

bool configValueCryptAlgorithmsParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueFormatCrypyAlgorithms
* Purpose: format crypt algorithm config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueCrypyAlgorithmsFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueCertificateParse
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

bool configValueCertificateParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueCertificateFormat
* Purpose: format server certificate config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueCertificateFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueKeyParse
* Purpose: config value option call back for parsing key
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored into variable, FALSE
*          otherwise
* Notes  : read from file or decode base64 data
\***********************************************************************/

bool configValueKeyParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
bool configValuePublicPrivateKeyParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : configValueKeyFormat
* Purpose: format key config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueKeyFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

/***********************************************************************\
* Name   : configValueHashDataParse
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

bool configValueHashDataParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : configValueFormatHashData
* Purpose: format hash data config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

//bool configValueHashDataFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

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
* Name   : readConfigFile
* Purpose: read configuration from file
* Input  : fileName      - file name
*          printInfoFlag - TRUE for output info, FALSE otherwise
* Output : -
* Return : TRUE iff configuration read, FALSE otherwise (error)
* Notes  : -
\***********************************************************************/

bool readConfigFile(ConstString fileName, bool printInfoFlag);

/***********************************************************************\
* Name   : updateConfig
* Purpose: update config file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors updateConfig(void);

/***********************************************************************\
* Name   : readCertificateFile
* Purpose: read certificate file
* Input  : certificate - certificate variable
*          fileName    - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors readCertificateFile(Certificate *certificate, const char *fileName);

/***********************************************************************\
* Name   : readKeyFile
* Purpose: read public/private key file
* Input  : key      - key variable
*          fileName - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors readKeyFile(Key *key, const char *fileName);

#ifdef __cplusplus
  }
#endif

#endif /* __CONFIGURATION__ */

/* end of file */
