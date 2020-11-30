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
#include "common/cmdoptions.h"
#include "common/configvalues.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/passwords.h"
#include "common/misc.h"

#include "entrylists.h"
#include "compress.h"
#include "crypt.h"
#include "bar_global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

extern CommandLineOption       COMMAND_LINE_OPTIONS[];

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

extern ConfigValue             JOB_CONFIG_VALUES[];

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
extern GlobalOptions   globalOptions;          // global options
extern GlobalOptionSet globalOptionSet;        // global option set
extern String          uuid;                   // BAR instance UUID

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
* Name   : Configuration_initKey
* Purpose: init empty public/private key
* Input  : key - key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_initKey(Key *key);

/***********************************************************************\
* Name   : Configuration_setKey
* Purpose: set public/private key
* Input  : key    - key
*          data   - key data
*          length - length of key data [bytes]
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool Configuration_setKey(Key *key, const void *data, uint length);

/***********************************************************************\
* Name   : Configuration_setKeyString
* Purpose: set public/private key with string
* Input  : key    - key
*          string - key data (PEM encoded)
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool Configuration_setKeyString(Key *key, ConstString string);

/***********************************************************************\
* Name   : Configuration_duplicateKey
* Purpose: duplicate public/private key
* Input  : key     - key variable
*          fromKey - from key
* Output : -
* Return : TRUE iff key duplicated
* Notes  : -
\***********************************************************************/

bool Configuration_duplicateKey(Key *key, const Key *fromKey);

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
* Name   : Configuration_initGlobalOptions
* Purpose: initialize global option values
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_initGlobalOptions(void);

/***********************************************************************\
* Name   : Configuration_doneGlobalOptions
* Purpose: deinitialize global option values
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_doneGlobalOptions(void);

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

/***********************************************************************\
* Name   : Configuration_newServerNode
* Purpose: new config file node
* Input  : name       - name
*          serverType - server type
* Output : -
* Return : server node
* Notes  : -
\***********************************************************************/

ServerNode *Configuration_newServerNode(ConstString name, ServerTypes serverType);

/***********************************************************************\
* Name   : Configuration_freeServerNode
* Purpose: free server node
* Input  : serverNode - server node
*          userData   - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Configuration_freeServerNode(ServerNode *serverNode, void *userData);

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
* Name   : updateConfig
* Purpose: update configuration file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Configuration_update(void);

/***********************************************************************\
* Name   : Configuration_readAllServerKeys
* Purpose: initialize all server keys/certificates
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Configuration_readAllServerKeys(void);

/***********************************************************************\
* Name   : Configuration_validate
* Purpose: validate options
* Input  : -
* Output : -
* Return : TRUE if options valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Configuration_validate(void);

#ifdef __cplusplus
  }
#endif

#endif /* __CONFIGURATION__ */

/* end of file */
