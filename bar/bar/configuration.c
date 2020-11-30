/***********************************************************************\
*
* $Revision: 11196 $
* $Date: 2020-11-26 14:24:42 +0100 (Thu, 26 Nov 2020) $
* $Author: torsten $
* Contents: Backup ARchiver configuration
* Systems: all
*
\***********************************************************************/

#define __CONFIGURATION_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
  #include <sys/resource.h>
#endif
#ifdef HAVE_LIBINTL_H
  #include <libintl.h>
#endif
#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/autofree.h"
#include "common/cmdoptions.h"
#include "common/configvalues.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/arrays.h"
#include "common/threads.h"
#include "common/files.h"
#include "common/patternlists.h"
#include "common/network.h"
#include "common/database.h"
#include "common/misc.h"
#include "common/passwords.h"

#include "errors.h"
#include "entrylists.h"
#include "deltasourcelists.h"
#include "compress.h"
#include "crypt.h"
#include "archive.h"
#include "storage.h"
#include "deltasources.h"
#include "index.h"
#include "continuous.h"
#if HAVE_BREAKPAD
  #include "minidump.h"
#endif /* HAVE_BREAKPAD */
#include "jobs.h"

#include "configuration.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
GlobalOptions        globalOptions;
GlobalOptionSet      globalOptionSet;
String               uuid;

bool                 configModified;

/*---------------------------------------------------------------------*/

LOCAL ConfigFileList configFileList;      // list of configuration files to read

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL const CommandLineUnit COMMAND_LINE_BYTES_UNITS[] = CMD_VALUE_UNIT_ARRAY
(
  {"T",1024LL*1024LL*1024LL*1024LL},
  {"G",1024LL*1024LL*1024LL},
  {"M",1024LL*1024LL},
  {"K",1024LL},
);

LOCAL const CommandLineUnit COMMAND_LINE_BITS_UNITS[] = CMD_VALUE_UNIT_ARRAY
(
  {"K",1024LL},
);

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_GENERATE_KEY_MODES[] = CMD_VALUE_SELECT_ARRAY
(
  {"secure",   CRYPT_KEY_MODE_NONE,     "secure keys"                 },
  {"transient",CRYPT_KEY_MODE_TRANSIENT,"transient keys (less secure)"},
);

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_SERVER_MODES[] = CMD_VALUE_SELECT_ARRAY
(
  {"master",SERVER_MODE_MASTER,"master"},
  {"slave", SERVER_MODE_SLAVE, "slave" },
);

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_PATTERN_TYPES[] = CMD_VALUE_SELECT_ARRAY
(
  {"glob",    PATTERN_TYPE_GLOB,          "glob patterns: * and ?"                      },
  {"regex",   PATTERN_TYPE_REGEX,         "regular expression pattern matching"         },
  {"extended",PATTERN_TYPE_EXTENDED_REGEX,"extended regular expression pattern matching"},
);

LOCAL const CommandLineOptionSelect COMPRESS_ALGORITHMS_DELTA[] = CMD_VALUE_SELECT_ARRAY
(
  {"none", COMPRESS_ALGORITHM_NONE,NULL},

  #ifdef HAVE_XDELTA
    {"xdelta1",COMPRESS_ALGORITHM_XDELTA_1,NULL},
    {"xdelta2",COMPRESS_ALGORITHM_XDELTA_2,NULL},
    {"xdelta3",COMPRESS_ALGORITHM_XDELTA_3,NULL},
    {"xdelta4",COMPRESS_ALGORITHM_XDELTA_4,NULL},
    {"xdelta5",COMPRESS_ALGORITHM_XDELTA_5,NULL},
    {"xdelta6",COMPRESS_ALGORITHM_XDELTA_6,NULL},
    {"xdelta7",COMPRESS_ALGORITHM_XDELTA_7,NULL},
    {"xdelta8",COMPRESS_ALGORITHM_XDELTA_8,NULL},
    {"xdelta9",COMPRESS_ALGORITHM_XDELTA_9,NULL},
  #endif /* HAVE_XDELTA */
);

LOCAL CommandLineOptionSelect COMPRESS_ALGORITHMS_BYTE[] = CMD_VALUE_SELECT_ARRAY
(
  {"none",COMPRESS_ALGORITHM_NONE,NULL},

  {"zip0",COMPRESS_ALGORITHM_ZIP_0,NULL},
  {"zip1",COMPRESS_ALGORITHM_ZIP_1,NULL},
  {"zip2",COMPRESS_ALGORITHM_ZIP_2,NULL},
  {"zip3",COMPRESS_ALGORITHM_ZIP_3,NULL},
  {"zip4",COMPRESS_ALGORITHM_ZIP_4,NULL},
  {"zip5",COMPRESS_ALGORITHM_ZIP_5,NULL},
  {"zip6",COMPRESS_ALGORITHM_ZIP_6,NULL},
  {"zip7",COMPRESS_ALGORITHM_ZIP_7,NULL},
  {"zip8",COMPRESS_ALGORITHM_ZIP_8,NULL},
  {"zip9",COMPRESS_ALGORITHM_ZIP_9,NULL},

  #ifdef HAVE_BZ2
    {"bzip1",COMPRESS_ALGORITHM_BZIP2_1,NULL},
    {"bzip2",COMPRESS_ALGORITHM_BZIP2_2,NULL},
    {"bzip3",COMPRESS_ALGORITHM_BZIP2_3,NULL},
    {"bzip4",COMPRESS_ALGORITHM_BZIP2_4,NULL},
    {"bzip5",COMPRESS_ALGORITHM_BZIP2_5,NULL},
    {"bzip6",COMPRESS_ALGORITHM_BZIP2_6,NULL},
    {"bzip7",COMPRESS_ALGORITHM_BZIP2_7,NULL},
    {"bzip8",COMPRESS_ALGORITHM_BZIP2_8,NULL},
    {"bzip9",COMPRESS_ALGORITHM_BZIP2_9,NULL},
  #endif /* HAVE_BZ2 */

  #ifdef HAVE_LZMA
    {"lzma1",COMPRESS_ALGORITHM_LZMA_1,NULL},
    {"lzma2",COMPRESS_ALGORITHM_LZMA_2,NULL},
    {"lzma3",COMPRESS_ALGORITHM_LZMA_3,NULL},
    {"lzma4",COMPRESS_ALGORITHM_LZMA_4,NULL},
    {"lzma5",COMPRESS_ALGORITHM_LZMA_5,NULL},
    {"lzma6",COMPRESS_ALGORITHM_LZMA_6,NULL},
    {"lzma7",COMPRESS_ALGORITHM_LZMA_7,NULL},
    {"lzma8",COMPRESS_ALGORITHM_LZMA_8,NULL},
    {"lzma9",COMPRESS_ALGORITHM_LZMA_9,NULL},
  #endif /* HAVE_LZMA */

  #ifdef HAVE_LZO
    {"lzo1",COMPRESS_ALGORITHM_LZO_1,NULL},
    {"lzo2",COMPRESS_ALGORITHM_LZO_2,NULL},
    {"lzo3",COMPRESS_ALGORITHM_LZO_3,NULL},
    {"lzo4",COMPRESS_ALGORITHM_LZO_4,NULL},
    {"lzo5",COMPRESS_ALGORITHM_LZO_5,NULL},
  #endif /* HAVE_LZO */

  #ifdef HAVE_LZ4
    {"lz4-0", COMPRESS_ALGORITHM_LZ4_0, NULL},
    {"lz4-1", COMPRESS_ALGORITHM_LZ4_1, NULL},
    {"lz4-2", COMPRESS_ALGORITHM_LZ4_2, NULL},
    {"lz4-3", COMPRESS_ALGORITHM_LZ4_3, NULL},
    {"lz4-4", COMPRESS_ALGORITHM_LZ4_4, NULL},
    {"lz4-5", COMPRESS_ALGORITHM_LZ4_5, NULL},
    {"lz4-6", COMPRESS_ALGORITHM_LZ4_6, NULL},
    {"lz4-7", COMPRESS_ALGORITHM_LZ4_7, NULL},
    {"lz4-8", COMPRESS_ALGORITHM_LZ4_8, NULL},
    {"lz4-9", COMPRESS_ALGORITHM_LZ4_9, NULL},
    {"lz4-10",COMPRESS_ALGORITHM_LZ4_10,NULL},
    {"lz4-11",COMPRESS_ALGORITHM_LZ4_11,NULL},
    {"lz4-12",COMPRESS_ALGORITHM_LZ4_12,NULL},
    {"lz4-13",COMPRESS_ALGORITHM_LZ4_13,NULL},
    {"lz4-14",COMPRESS_ALGORITHM_LZ4_14,NULL},
    {"lz4-15",COMPRESS_ALGORITHM_LZ4_15,NULL},
    {"lz4-16",COMPRESS_ALGORITHM_LZ4_16,NULL},
  #endif /* HAVE_LZ4 */

  #ifdef HAVE_ZSTD
    {"zstd0", COMPRESS_ALGORITHM_ZSTD_0 ,NULL},
    {"zstd1", COMPRESS_ALGORITHM_ZSTD_1 ,NULL},
    {"zstd2", COMPRESS_ALGORITHM_ZSTD_2 ,NULL},
    {"zstd3", COMPRESS_ALGORITHM_ZSTD_3 ,NULL},
    {"zstd4", COMPRESS_ALGORITHM_ZSTD_4 ,NULL},
    {"zstd5", COMPRESS_ALGORITHM_ZSTD_5 ,NULL},
    {"zstd6", COMPRESS_ALGORITHM_ZSTD_6 ,NULL},
    {"zstd7", COMPRESS_ALGORITHM_ZSTD_7 ,NULL},
    {"zstd8", COMPRESS_ALGORITHM_ZSTD_8 ,NULL},
    {"zstd9", COMPRESS_ALGORITHM_ZSTD_9 ,NULL},
    {"zstd10",COMPRESS_ALGORITHM_ZSTD_10,NULL},
    {"zstd11",COMPRESS_ALGORITHM_ZSTD_11,NULL},
    {"zstd12",COMPRESS_ALGORITHM_ZSTD_12,NULL},
    {"zstd13",COMPRESS_ALGORITHM_ZSTD_13,NULL},
    {"zstd14",COMPRESS_ALGORITHM_ZSTD_14,NULL},
    {"zstd15",COMPRESS_ALGORITHM_ZSTD_15,NULL},
    {"zstd16",COMPRESS_ALGORITHM_ZSTD_16,NULL},
    {"zstd17",COMPRESS_ALGORITHM_ZSTD_17,NULL},
    {"zstd18",COMPRESS_ALGORITHM_ZSTD_18,NULL},
    {"zstd19",COMPRESS_ALGORITHM_ZSTD_19,NULL},
  #endif /* HAVE_ZSTD */
);

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_CRYPT_TYPES[] = CMD_VALUE_SELECT_ARRAY
(
  #ifdef HAVE_GCRYPT
    {"symmetric", CRYPT_TYPE_SYMMETRIC, "symmetric"},
    {"asymmetric",CRYPT_TYPE_ASYMMETRIC,"asymmetric"},
  #endif /* HAVE_GCRYPT */
);

LOCAL const CommandLineUnit COMMAND_LINE_TIME_UNITS[] = CMD_VALUE_UNIT_ARRAY
(
  {"weeks",7*24*60*60},
  {"week",7*24*60*60},
  {"days",24*60*60},
  {"day",24*60*60},
  {"h",60*60},
  {"m",60},
  {"s",1},
);

LOCAL const CommandLineOptionSet COMMAND_LINE_OPTIONS_LOG_TYPES[] = CMD_VALUE_SET_ARRAY
(
  {"none",      LOG_TYPE_NONE,               "no logging"               },
  {"errors",    LOG_TYPE_ERROR,              "log errors"               },
  {"warnings",  LOG_TYPE_WARNING,            "log warnings"             },
  {"info",      LOG_TYPE_INFO,               "log info"                 },

  {"ok",        LOG_TYPE_ENTRY_OK,           "log stored/restored files"},
  {"unknown",   LOG_TYPE_ENTRY_TYPE_UNKNOWN, "log unknown files"        },
  {"skipped",   LOG_TYPE_ENTRY_ACCESS_DENIED,"log skipped files"        },
  {"missing",   LOG_TYPE_ENTRY_MISSING,      "log missing files"        },
  {"incomplete",LOG_TYPE_ENTRY_INCOMPLETE,   "log incomplete files"     },
  {"excluded",  LOG_TYPE_ENTRY_EXCLUDED,     "log excluded files"       },

  {"storage",   LOG_TYPE_STORAGE,            "log storage"              },

  {"index",     LOG_TYPE_INDEX,              "index database"           },

  {"continuous",LOG_TYPE_CONTINUOUS,         "continuous backup"        },

  {"all",       LOG_TYPE_ALL,                "log everything"           },
);

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_ARCHIVE_FILE_MODES[] = CMD_VALUE_SELECT_ARRAY
(
  {"stop",      ARCHIVE_FILE_MODE_STOP,      "stop if archive file exists"      },
  {"append",    ARCHIVE_FILE_MODE_APPEND,    "append to existing archive files" },
  {"overwrite", ARCHIVE_FILE_MODE_OVERWRITE, "overwrite existing archive files" },
);

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_RESTORE_ENTRY_MODES[] = CMD_VALUE_SELECT_ARRAY
(
  {"stop",      RESTORE_ENTRY_MODE_STOP,      "stop if entry exists" },
  {"rename",    RESTORE_ENTRY_MODE_RENAME,    "rename entries"       },
  {"overwrite", RESTORE_ENTRY_MODE_OVERWRITE, "overwrite entries"    },
);

const ConfigValueUnit CONFIG_VALUE_BYTES_UNITS[] = CONFIG_VALUE_UNIT_ARRAY
(
  {"T",1024LL*1024LL*1024LL*1024LL},
  {"G",1024LL*1024LL*1024LL},
  {"M",1024LL*1024LL},
  {"K",1024LL},
);

const ConfigValueUnit CONFIG_VALUE_BITS_UNITS[] = CONFIG_VALUE_UNIT_ARRAY
(
  {"K",1024LL},
);

const ConfigValueSelect CONFIG_VALUE_ARCHIVE_TYPES[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"normal",      ARCHIVE_TYPE_NORMAL,     },
  {"full",        ARCHIVE_TYPE_FULL,       },
  {"incremental", ARCHIVE_TYPE_INCREMENTAL },
  {"differential",ARCHIVE_TYPE_DIFFERENTIAL},
  {"continuous",  ARCHIVE_TYPE_CONTINUOUS  },
);

const ConfigValueSelect CONFIG_VALUE_PATTERN_TYPES[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"glob",    PATTERN_TYPE_GLOB,         },
  {"regex",   PATTERN_TYPE_REGEX,        },
  {"extended",PATTERN_TYPE_EXTENDED_REGEX},
);

const ConfigValueSelect CONFIG_VALUE_CRYPT_TYPES[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"none", CRYPT_TYPE_NONE },

  #ifdef HAVE_GCRYPT
    {"symmetric", CRYPT_TYPE_SYMMETRIC },
    {"asymmetric",CRYPT_TYPE_ASYMMETRIC},
  #endif /* HAVE_GCRYPT */
);

const ConfigValueSelect CONFIG_VALUE_PASSWORD_MODES[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"default",PASSWORD_MODE_DEFAULT,},
  {"ask",    PASSWORD_MODE_ASK,    },
  {"config", PASSWORD_MODE_CONFIG, },
);

const ConfigValueUnit CONFIG_VALUE_TIME_UNITS[] = CONFIG_VALUE_UNIT_ARRAY
(
  {"weeks",7*24*60*60},
  {"week",7*24*60*60},
  {"days",24*60*60},
  {"day",24*60*60},
  {"h",60*60},
  {"m",60},
  {"s",1},
);

const ConfigValueSet CONFIG_VALUE_LOG_TYPES[] = CONFIG_VALUE_SET_ARRAY
(
  {"none",      LOG_TYPE_NONE               },
  {"errors",    LOG_TYPE_ERROR              },
  {"warnings",  LOG_TYPE_WARNING            },
  {"info",      LOG_TYPE_INFO               },

  {"ok",        LOG_TYPE_ENTRY_OK           },
  {"unknown",   LOG_TYPE_ENTRY_TYPE_UNKNOWN },
  {"skipped",   LOG_TYPE_ENTRY_ACCESS_DENIED},
  {"missing",   LOG_TYPE_ENTRY_MISSING      },
  {"incomplete",LOG_TYPE_ENTRY_INCOMPLETE   },
  {"excluded",  LOG_TYPE_ENTRY_EXCLUDED     },

  {"storage",   LOG_TYPE_STORAGE            },

  {"index",     LOG_TYPE_INDEX,             },

  {"continuous",LOG_TYPE_CONTINUOUS,        },

  {"all",       LOG_TYPE_ALL                },
);

const ConfigValueSelect CONFIG_VALUE_ARCHIVE_FILE_MODES[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"stop",      ARCHIVE_FILE_MODE_STOP      },
  {"append",    ARCHIVE_FILE_MODE_APPEND    },
  {"overwrite", ARCHIVE_FILE_MODE_OVERWRITE },
);

const ConfigValueSelect CONFIG_VALUE_RESTORE_ENTRY_MODES[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"stop",      RESTORE_ENTRY_MODE_STOP      },
  {"rename",    RESTORE_ENTRY_MODE_RENAME    },
  {"overwrite", RESTORE_ENTRY_MODE_OVERWRITE },
);

const ConfigValueSelect CONFIG_VALUE_SERVER_MODES[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"master", SERVER_MODE_MASTER},
  {"slave",  SERVER_MODE_SLAVE },
);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : newConfigFileNode
* Purpose: new config file node
* Input  : configFileType - config file type
*          fileName       - file name
* Output : -
* Return : config file node
* Notes  : -
\***********************************************************************/

LOCAL ConfigFileNode* newConfigFileNode(ConfigFileTypes configFileType, const char *fileName)
{
  ConfigFileNode *configFileNode;

  assert(fileName != NULL);

  configFileNode = LIST_NEW_NODE(ConfigFileNode);
  if (configFileNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  configFileNode->type     = configFileType;
  configFileNode->fileName = String_newCString(fileName);

  return configFileNode;
}

/***********************************************************************\
* Name   : freeConfigFileNode
* Purpose: free config file node
* Input  : configFileNode - config file node
*          userData       - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeConfigFileNode(ConfigFileNode *configFileNode, void *userData)
{
  assert(configFileNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(configFileNode->fileName);
}

/***********************************************************************\
* Name   : initDevice
* Purpose: init device
* Input  : device - device
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initDevice(Device *device)
{
  assert(device != NULL);

  device->name                    = NULL;
  device->requestVolumeCommand    = NULL;
  device->unloadVolumeCommand     = NULL;
  device->loadVolumeCommand       = NULL;
  device->volumeSize              = 0LL;
  device->imagePreProcessCommand  = NULL;
  device->imagePostProcessCommand = NULL;
  device->imageCommand            = NULL;
  device->eccPreProcessCommand    = NULL;
  device->eccPostProcessCommand   = NULL;
  device->eccCommand              = NULL;
  device->blankCommand            = NULL;
  device->writePreProcessCommand  = NULL;
  device->writePostProcessCommand = NULL;
  device->writeCommand            = NULL;
}

/***********************************************************************\
* Name   : doneDevice
* Purpose: done device
* Input  : device - device
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneDevice(Device *device)
{
  assert(device != NULL);

  String_delete(device->writeCommand           );
  String_delete(device->writePostProcessCommand);
  String_delete(device->writePreProcessCommand );
  String_delete(device->blankCommand           );
  String_delete(device->eccCommand             );
  String_delete(device->eccPostProcessCommand  );
  String_delete(device->eccPreProcessCommand   );
  String_delete(device->imageCommand           );
  String_delete(device->imagePostProcessCommand);
  String_delete(device->imagePreProcessCommand );
  String_delete(device->loadVolumeCommand      );
  String_delete(device->unloadVolumeCommand    );
  String_delete(device->requestVolumeCommand   );
  String_delete(device->name                   );
}

/***********************************************************************\
* Name   : newDeviceNode
* Purpose: new server node
* Input  : name - device name
* Output : -
* Return : device node
* Notes  : -
\***********************************************************************/

LOCAL DeviceNode *newDeviceNode(ConstString name)
{
  DeviceNode *deviceNode;

  // allocate new device node
  deviceNode = LIST_NEW_NODE(DeviceNode);
  if (deviceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  initDevice(&deviceNode->device);

  deviceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  deviceNode->device.name = String_duplicate(name);

  return deviceNode;
}

/***********************************************************************\
* Name   : freeDeviceNode
* Purpose: free device node
* Input  : deviceNode - device node
*          userData   - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeDeviceNode(DeviceNode *deviceNode, void *userData)
{
  assert(deviceNode != NULL);

  UNUSED_VARIABLE(userData);

  doneDevice(&deviceNode->device);
}

/***********************************************************************\
* Name   : freeMaintenanceNode
* Purpose: free maintenace node
* Input  : maintenanceNode - maintenance node
*          userData        - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeMaintenanceNode(MaintenanceNode *maintenanceNode, void *userData)
{
  assert(maintenanceNode != NULL);

  UNUSED_VARIABLE(maintenanceNode);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : newMountNodeCString
* Purpose: new mount node
* Input  : mountName  - mount name
*          deviceName - device name
* Output : -
* Return : mount node
* Notes  : -
\***********************************************************************/

LOCAL MountNode *newMountNodeCString(const char *mountName, const char *deviceName)
{
  MountNode *mountNode;

  assert(mountName != NULL);
  assert(!stringIsEmpty(mountName));

  // allocate mount node
  mountNode = LIST_NEW_NODE(MountNode);
  if (mountNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  mountNode->id            = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  mountNode->name          = String_newCString(mountName);
  mountNode->device        = String_newCString(deviceName);
  mountNode->mounted       = FALSE;
  mountNode->mountCount    = 0;

  return mountNode;
}

/***********************************************************************\
* Name   : newMountNode
* Purpose: new mount node
* Input  : mountName  - mount name
*          deviceName - device name
* Output : -
* Return : mount node
* Notes  : -
\***********************************************************************/

LOCAL MountNode *newMountNode(ConstString mountName, ConstString deviceName)
{
  assert(mountName != NULL);

  return newMountNodeCString(String_cString(mountName),
                             String_cString(deviceName)
                            );
}

/***********************************************************************\
* Name   : duplicateMountNode
* Purpose: duplicate mount node
* Input  : fromMountNode - from mount name
*          userData      - user data
* Output : -
* Return : mount node
* Notes  : -
\***********************************************************************/

LOCAL MountNode *duplicateMountNode(MountNode *fromMountNode,
                                    void      *userData
                                   )
{
  MountNode *mountNode;

  assert(fromMountNode != NULL);

  UNUSED_VARIABLE(userData);

  mountNode = newMountNode(fromMountNode->name,
                           fromMountNode->device
                          );
  assert(mountNode != NULL);

  return mountNode;
}

/***********************************************************************\
* Name   : freeMountNode
* Purpose: free mount node
* Input  : mountNode - mount node
*          userData  - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeMountNode(MountNode *mountNode, void *userData)
{
  assert(mountNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(mountNode->device);
  String_delete(mountNode->name);
}

/***********************************************************************\
* Name   : deleteMountNode
* Purpose: delete mount node
* Input  : mountNode - mount node
*          userData  - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteMountNode(MountNode *mountNode)
{
  assert(mountNode != NULL);

  freeMountNode(mountNode,NULL);
  LIST_DELETE_NODE(mountNode);
}

//TODO: required? remove?
#if 0
LOCAL Errors readCAFile(Certificate *certificate, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  uint64     dataLength;
  void       *data;

  assert(certificate != NULL);
  assert(fileName != NULL);

  certificate->data   = NULL;
  certificate->length = 0;

  error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get file size
  dataLength = File_getSize(&fileHandle);
  if (dataLength == 0LL)
  {
    (void)File_close(&fileHandle);
    return ERROR_NO_TLS_CA;
  }

  // allocate memory
  data = malloc((size_t)dataLength);
  if (data == NULL)
  {
    (void)File_close(&fileHandle);
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // read file data
  error = File_read(&fileHandle,
                    data,
                    dataLength,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    free(data);
    (void)File_close(&fileHandle);
    return error;
  }

  // close file
  (void)File_close(&fileHandle);

  // set certificate data
  if (certificate->data != NULL) free(certificate->data);
  certificate->data   = data;
  certificate->length = dataLength;

  return ERROR_NONE;
}
#endif

/***********************************************************************\
* Name   : cmdOptionParseString
* Purpose: command line option call back for parsing string
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseString(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if ((*(String*)variable) != NULL)
  {
    String_setCString(*(String*)variable,value);
  }
  else
  {
    (*(String*)variable) = String_newCString(value);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseConfigFile
* Purpose: command line option call back for parsing configuration file
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseConfigFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  ConfigFileNode *configFileNode;

  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  Configuration_add(CONFIG_FILE_TYPE_COMMAND_LINE,value);

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseEntryPattern
* Purpose: command line option call back for parsing include
*          patterns
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseEntryPattern(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  EntryTypes   entryType;
  PatternTypes patternType;
  Errors       error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  // get entry type
  entryType = ENTRY_TYPE_FILE;
  switch (globalOptions.command)
  {
    case COMMAND_NONE:
    case COMMAND_CREATE_FILES:
    case COMMAND_LIST:
    case COMMAND_TEST:
    case COMMAND_COMPARE:
    case COMMAND_RESTORE:
    case COMMAND_CONVERT:
    case COMMAND_GENERATE_ENCRYPTION_KEYS:
    case COMMAND_GENERATE_SIGNATURE_KEYS:
    case COMMAND_NEW_KEY_PASSWORD:
      entryType = ENTRY_TYPE_FILE;
      break;
    case COMMAND_CREATE_IMAGES:
      entryType = ENTRY_TYPE_IMAGE;
      break;
    default:
      HALT_INTERNAL_ERROR("no valid command set (%d)",globalOptions.command);
      break; // not reached
  }

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // append to list
  error = EntryList_appendCString((EntryList*)variable,entryType,value,patternType,NULL);
  if (error != ERROR_NONE)
  {
    stringSet(errorMessage,errorMessageSize,Error_getText(error));
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParsePattern
* Purpose: command line option call back for parsing patterns
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParsePattern(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  PatternTypes patternType;
  Errors       error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // append to list
  error = PatternList_appendCString((PatternList*)variable,value,patternType,NULL);
  if (error != ERROR_NONE)
  {
    stringSet(errorMessage,errorMessageSize,Error_getText(error));
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseMount
* Purpose: command line option call back for mounts
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseMount(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  String    mountName;
  String    deviceName;
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // init variables
  mountName  = String_new();
  deviceName = String_new();

  // get name
  if      (String_parseCString(value,"%S,%S,%y",NULL,mountName,deviceName,NULL))
  {
    // Note: %y deprecated
  }
  else if (String_parseCString(value,"%S,,%y",NULL,mountName,NULL))
  {
    // Note: %y deprecated
    String_clear(deviceName);
  }
  else if (String_parseCString(value,"%S,%y",NULL,mountName,NULL))
  {
    // Note: %y deprecated
    String_clear(deviceName);
  }
  else if (String_parseCString(value,"%S,%S",NULL,mountName,deviceName))
  {
  }
  else if (String_parseCString(value,"%S",NULL,mountName))
  {
    String_clear(deviceName);
  }
  else
  {
    String_delete(deviceName);
    String_delete(mountName);
    return FALSE;
  }

  if (!String_isEmpty(mountName))
  {
    // add to mount list
    mountNode = newMountNode(mountName,
                             deviceName
                            );
    assert(mountNode != NULL);
    List_append((MountList*)variable,mountNode);
  }

  // free resources
  String_delete(deviceName);
  String_delete(mountName);

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseDeltaSource
* Purpose: command line option call back for parsing delta patterns
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseDeltaSource(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  String       storageName;
  PatternTypes patternType;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // init variables
  storageName = String_new();

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { String_setCString(storageName,value+2); patternType = PATTERN_TYPE_REGEX;          }
  else if (strncmp(value,"x:",2) == 0) { String_setCString(storageName,value+2); patternType = PATTERN_TYPE_EXTENDED_REGEX; }
  else if (strncmp(value,"g:",2) == 0) { String_setCString(storageName,value+2); patternType = PATTERN_TYPE_GLOB;           }
  else                                 { String_setCString(storageName,value  ); patternType = PATTERN_TYPE_GLOB;           }

  // append to delta source list
  DeltaSourceList_append((DeltaSourceList*)variable,storageName,patternType,NULL);

  // free resources
  String_delete(storageName);

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseCompressAlgorithms
* Purpose: command line option call back for parsing compress algorithm
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseCompressAlgorithms(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  char               algorithm1[256],algorithm2[256];
  CompressAlgorithms compressAlgorithm;
  CompressAlgorithms compressAlgorithmDelta,compressAlgorithmByte;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  compressAlgorithmDelta = COMPRESS_ALGORITHM_NONE;
  compressAlgorithmByte  = COMPRESS_ALGORITHM_NONE;

  // parse
  if (   String_scanCString(value,"%256s+%256s",algorithm1,algorithm2)
      || String_scanCString(value,"%256s,%256s",algorithm1,algorithm2)
     )
  {
    if (!Compress_parseAlgorithm(algorithm1,&compressAlgorithm))
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",algorithm1);
      return FALSE;
    }
    if      (Compress_isDeltaCompressed(compressAlgorithm)) compressAlgorithmDelta = compressAlgorithm;
    else if (Compress_isByteCompressed (compressAlgorithm)) compressAlgorithmByte  = compressAlgorithm;

    if (!Compress_parseAlgorithm(algorithm2,&compressAlgorithm))
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",algorithm2);
      return FALSE;
    }
    if      (Compress_isDeltaCompressed(compressAlgorithm)) compressAlgorithmDelta = compressAlgorithm;
    else if (Compress_isByteCompressed (compressAlgorithm)) compressAlgorithmByte  = compressAlgorithm;
  }
  else
  {
    if (!Compress_parseAlgorithm(value,&compressAlgorithm))
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",value);
      return FALSE;
    }
    if      (Compress_isDeltaCompressed(compressAlgorithm)) compressAlgorithmDelta = compressAlgorithm;
    else if (Compress_isByteCompressed (compressAlgorithm)) compressAlgorithmByte  = compressAlgorithm;
  }

  // store compress algorithm values
  ((CompressAlgorithmsDeltaByte*)variable)->delta = compressAlgorithmDelta;
  ((CompressAlgorithmsDeltaByte*)variable)->byte  = compressAlgorithmByte;

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseCryptAlgorithms
* Purpose: command line option call back for parsing crypt algorithms
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseCryptAlgorithms(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
#ifdef MULTI_CRYPT
  StringTokenizer stringTokenizer;
  ConstString     token;
//  CryptAlgorithms cryptAlgorithms[4];
  uint            cryptAlgorithmCount;
#else /* not MULTI_CRYPT */
  CryptAlgorithms cryptAlgorithm;
#endif /* MULTI_CRYPT */

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

#ifdef MULTI_CRYPT
  cryptAlgorithmCount = 0;
  String_initTokenizerCString(&stringTokenizer,
                              value,
                              "+,",
                              STRING_QUOTES,
                              TRUE
                             );
  while (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    if (cryptAlgorithmCount >= 4)
    {
      stringFormat(errorMessage,errorMessageSize,"Too many crypt algorithms (max. 4)");
      String_doneTokenizer(&stringTokenizer);
      return FALSE;
    }

    if (!Crypt_parseAlgorithm(String_cString(token),&((JobOptionCryptAlgorithms*)variable)->values[cryptAlgorithmCount]))
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown crypt algorithm '%s'",String_cString(token));
      String_doneTokenizer(&stringTokenizer);
      return FALSE;
    }

    if (((JobOptionCryptAlgorithms*)variable)->values[cryptAlgorithmCount] != CRYPT_ALGORITHM_NONE)
    {
      cryptAlgorithmCount++;
    }
  }
  String_doneTokenizer(&stringTokenizer);
  while (cryptAlgorithmCount < 4)
  {
    ((JobOptionCryptAlgorithms*)variable)->values[cryptAlgorithmCount] = CRYPT_ALGORITHM_NONE;
    cryptAlgorithmCount++;
  }
#else /* not MULTI_CRYPT */
  // parse
  if (!Crypt_parseAlgorithm(value,&cryptAlgorithm))
  {
    stringFormat(errorMessage,errorMessageSize,"Unknown crypt algorithm '%s'",value);
    return FALSE;
  }

  // store crypt algorithm
  ((CryptAlgorithms*)variable)[0] = cryptAlgorithm;
#endif /* MULTI_CRYPT */

  return TRUE;
}

/***********************************************************************\
* Name   : parseBandWidthNumber
* Purpose: parse band width number
* Input  : s - string to parse
*          commandLineUnits
* Output : value - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseBandWidthNumber(ConstString s, ulong *n)
{
  const StringUnit UNITS[] =
  {
    {"T",1024LL*1024LL*1024LL*1024LL},
    {"G",1024LL*1024LL*1024LL},
    {"M",1024LL*1024LL},
    {"K",1024LL},
  };

  assert(s != NULL);
  assert(n != NULL);

  (*n) = (ulong)String_toInteger64(s,STRING_BEGIN,NULL,UNITS,SIZE_OF_ARRAY(UNITS));

  return TRUE;
}

/***********************************************************************\
* Name   : newBandWidthNode
* Purpose: create new band width node
* Input  : -
* Output : -
* Return : band width node
* Notes  : -
\***********************************************************************/

LOCAL BandWidthNode *newBandWidthNode(void)
{
  BandWidthNode *bandWidthNode;

  bandWidthNode = LIST_NEW_NODE(BandWidthNode);
  if (bandWidthNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  bandWidthNode->year        = DATE_ANY;
  bandWidthNode->month       = DATE_ANY;
  bandWidthNode->day         = DATE_ANY;
  bandWidthNode->hour        = TIME_ANY;
  bandWidthNode->minute      = TIME_ANY;
  bandWidthNode->weekDaySet  = WEEKDAY_SET_ANY;
  bandWidthNode->n           = 0L;
  bandWidthNode->fileName    = NULL;

  return bandWidthNode;
}

/***********************************************************************\
* Name   : deleteBandWidthNode
* Purpose: delete band width node
* Input  : bandWidthNode - band width node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteBandWidthNode(BandWidthNode *bandWidthNode)
{
  assert(bandWidthNode != NULL);

  String_delete(bandWidthNode->fileName);
  LIST_DELETE_NODE(bandWidthNode);
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : initCertificate
* Purpose: init empty certificate
* Input  : certificate - certificate
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initCertificate(Certificate *certificate)
{
  assert(certificate != NULL);

  certificate->type     = CERTIFICATE_TYPE_NONE;
  certificate->fileName = String_new();
  certificate->data     = NULL;
  certificate->length   = 0;
}

/***********************************************************************\
* Name   : duplicateCertificate
* Purpose: duplicate certificate
* Input  : toCertificate   - certificate variable
*          fromCertificate - from certificate
* Output : -
* Return : TRUE iff certificate duplicated
* Notes  : -
\***********************************************************************/

LOCAL bool duplicateCertificate(Certificate *toCertificate, const Certificate *fromCertificate)
{
  void *data;

  assert(toCertificate != NULL);
  assert(fromCertificate != NULL);

  data = malloc(fromCertificate->length);
  if (data == NULL)
  {
    return FALSE;
  }
  memCopyFast(data,fromCertificate->length,fromCertificate->data,fromCertificate->length);

  toCertificate->type     = fromCertificate->type;
  toCertificate->fileName = String_duplicate(fromCertificate->fileName);
  toCertificate->data     = data;
  toCertificate->length   = fromCertificate->length;

  return TRUE;
}

/***********************************************************************\
* Name   : doneCertificate
* Purpose: free certificate
* Input  : certificate - certificate
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneCertificate(Certificate *certificate)
{
  assert(certificate != NULL);

  if (certificate->data != NULL)
  {
    free(certificate->data);
  }
  String_delete(certificate->fileName);
}

/***********************************************************************\
* Name   : clearCertificate
* Purpose: clear certificate
* Input  : certificate - certificate
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearCertificate(Certificate *certificate)
{
  assert(certificate != NULL);

  if (certificate->data != NULL) free(certificate->data);
  certificate->data   = NULL;
  certificate->length = 0;
}

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

LOCAL bool setCertificate(Certificate *certificate, const void *certificateData, uint certificateLength)
{
  void *data;

  assert(certificate != NULL);

  data = malloc(certificateLength);
  if (data == NULL)
  {
    return FALSE;
  }
  memCopyFast(data,certificateLength,certificateData,certificateLength);

  if (certificate->data != NULL) free(certificate->data);
  certificate->data   = data;
  certificate->length = certificateLength;

  return TRUE;
}

/***********************************************************************\
* Name   : setCertificateString
* Purpose: set certificate with string
* Input  : certificate - certificate
*          string      - certificate data
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

LOCAL bool setCertificateString(Certificate *certificate, ConstString string)
{
  return setCertificate(certificate,String_cString(string),String_length(string));
}

// ----------------------------------------------------------------------

void Configuration_initKey(Key *key)
{
  assert(key != NULL);

  key->data   = NULL;
  key->length = 0;
}

bool Configuration_setKey(Key *key, const void *data, uint length)
{
  void *newData;

  assert(key != NULL);

  newData = allocSecure(length);
  if (newData == NULL)
  {
    return FALSE;
  }
  memCopyFast(newData,length,data,length);

  if (key->data != NULL) freeSecure(key->data);
  key->data   = newData;
  key->length = length;

  return TRUE;
}

bool Configuration_setKeyString(Key *key, ConstString string)
{
  return Configuration_setKey(key,String_cString(string),String_length(string));
}

bool Configuration_duplicateKey(Key *key, const Key *fromKey)
{
  uint length;
  void *data;

  assert(key != NULL);

  if ((fromKey != NULL) && (fromKey->length > 0))
  {
    length = fromKey->length;
    data = allocSecure(length);
    if (data == NULL)
    {
      return FALSE;
    }
    memCopyFast(data,length,fromKey->data,length);
  }
  else
  {
    data   = NULL;
    length = 0;
  }

  key->data   = data;
  key->length = length;

  return TRUE;
}

void Configuration_doneKey(Key *key)
{
  assert(key != NULL);

  if (key->data != NULL) freeSecure(key->data);
  key->data   = NULL;
  key->length = 0;
}

void Configuration_clearKey(Key *key)
{
  assert(key != NULL);

  Configuration_doneKey(key);
}

bool Configuration_copyKey(Key *key, const Key *fromKey)
{
  uint length;
  void *data;

  assert(key != NULL);

  if (fromKey != NULL)
  {
    length = fromKey->length;
    data = allocSecure(length);
    if (data == NULL)
    {
      return FALSE;
    }
    memCopyFast(data,length,fromKey->data,length);
  }
  else
  {
    data   = NULL;
    length = 0;
  }

  if (key->data != NULL) freeSecure(key->data);
  key->data   = data;
  key->length = length;

  return TRUE;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : initHash
* Purpose: init hash
* Input  : hash - hash variable
* Output : hash - empty hash
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initHash(Hash *hash)
{
  assert(hash != NULL);

  hash->cryptHashAlgorithm = CRYPT_HASH_ALGORITHM_NONE;
  hash->data               = NULL;
  hash->length             = 0;
}

/***********************************************************************\
* Name   : doneHash
* Purpose: done hash
* Input  : hash - hash
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneHash(Hash *hash)
{
  assert(hash != NULL);

  if (hash->data != NULL) freeSecure(hash->data);
  hash->cryptHashAlgorithm = CRYPT_HASH_ALGORITHM_NONE;
  hash->data               = NULL;
  hash->length             = 0;
}

bool Configuration_setHash(Hash *hash, const CryptHash *cryptHash)
{
  uint length;
  void *data;

  assert(hash != NULL);
  assert(cryptHash != NULL);

  length = Crypt_getHashLength(cryptHash);
  data   = allocSecure(length);
  if (data == NULL)
  {
    return FALSE;
  }
  Crypt_getHash(cryptHash,data,length,NULL);

  if (hash->data != NULL) freeSecure(hash->data);
  hash->cryptHashAlgorithm = cryptHash->cryptHashAlgorithm;
  hash->data               = data;
  hash->length             = length;

  return TRUE;
}

void Configuration_clearHash(Hash *hash)
{
  assert(hash != NULL);

  doneHash(hash);
}

bool Configuration_equalsHash(Hash *hash, const CryptHash *cryptHash)
{
  assert(hash != NULL);
  assert(cryptHash != NULL);

  return    (hash->cryptHashAlgorithm == cryptHash->cryptHashAlgorithm)
         && Crypt_equalsHashBuffer(cryptHash,
                                   hash->data,
                                   hash->length
                                  );
}

// ----------------------------------------------------------------------

void initServer(Server *server, ConstString name, ServerTypes serverType)
{
  assert(server != NULL);

  server->name = (name != NULL) ? String_duplicate(name) : String_new();
  server->type = serverType;
  switch (serverType)
  {
    case SERVER_TYPE_NONE:
      break;
    case SERVER_TYPE_FILE:
      break;
    case SERVER_TYPE_FTP:
      server->ftp.loginName = String_new();
      Password_init(&server->ftp.password);
      break;
    case SERVER_TYPE_SSH:
      server->ssh.port      = 22;
      server->ssh.loginName = String_new();
      Password_init(&server->ssh.password);
      Configuration_initKey(&server->ssh.publicKey);
      Configuration_initKey(&server->ssh.privateKey);
      break;
    case SERVER_TYPE_WEBDAV:
      server->webDAV.loginName = String_new();
      Password_init(&server->webDAV.password);
      Configuration_initKey(&server->webDAV.publicKey);
      Configuration_initKey(&server->webDAV.privateKey);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; // not reached
    #endif /* NDEBUG */
  }
  server->maxConnectionCount      = 0;
  server->maxStorageSize          = 0;
  server->writePreProcessCommand  = String_new();
  server->writePostProcessCommand = String_new();
}

void doneServer(Server *server)
{
  assert(server != NULL);

  switch (server->type)
  {
    case SERVER_TYPE_NONE:
      break;
    case SERVER_TYPE_FILE:
      break;
    case SERVER_TYPE_FTP:
      Password_done(&server->ftp.password);
      String_delete(server->ftp.loginName);
      break;
    case SERVER_TYPE_SSH:
      if (Configuration_isKeyAvailable(&server->ssh.privateKey)) Configuration_doneKey(&server->ssh.privateKey);
      if (Configuration_isKeyAvailable(&server->ssh.publicKey)) Configuration_doneKey(&server->ssh.publicKey);
      Password_done(&server->ssh.password);
      String_delete(server->ssh.loginName);
      break;
    case SERVER_TYPE_WEBDAV:
      if (Configuration_isKeyAvailable(&server->webDAV.privateKey)) Configuration_doneKey(&server->webDAV.privateKey);
      if (Configuration_isKeyAvailable(&server->webDAV.publicKey)) Configuration_doneKey(&server->webDAV.publicKey);
      Password_done(&server->webDAV.password);
      String_delete(server->webDAV.loginName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; // not reached
    #endif /* NDEBUG */
  }
  String_delete(server->writePostProcessCommand);
  String_delete(server->writePreProcessCommand);
  String_delete(server->name);
}

uint getServerSettings(Server                 *server,
                       const StorageSpecifier *storageSpecifier,
                       const JobOptions       *jobOptions
                      )
{
  uint       serverId;
  ServerNode *serverNode;

  assert(server != NULL);
  assert(storageSpecifier != NULL);

  // get default settings
  serverId                        = 0;
  server->type                    = SERVER_TYPE_NONE;
  server->name                    = NULL;
  server->maxConnectionCount      = 0;
  server->maxStorageSize          = (jobOptions != NULL) ? jobOptions->maxStorageSize : 0LL;
  server->writePreProcessCommand  = NULL;
  server->writePostProcessCommand = NULL;

  // get server specific settings
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find file server
        serverNode = LIST_FIND(&globalOptions.serverList,
                               serverNode,
                                  (serverNode->type == SERVER_TYPE_FILE)
                               && String_startsWith(serverNode->name,storageSpecifier->archiveName)
                              );

        if (serverNode != NULL)
        {
          // get file server settings
          serverId = serverNode->id;
          initServer(server,serverNode->name,SERVER_TYPE_FILE);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->writePreProcessCommand )
                                                               ? serverNode->writePreProcessCommand
                                                               : globalOptions.file.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->writePostProcessCommand)
                                                               ? serverNode->writePostProcessCommand
                                                               : globalOptions.file.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find file server
        serverNode = LIST_FIND(&globalOptions.serverList,
                               serverNode,
                                  (serverNode->type == SERVER_TYPE_FTP)
                               && String_equals(serverNode->name,storageSpecifier->hostName)
                              );

        if (serverNode != NULL)
        {
          // get FTP server settings
          serverId  = serverNode->id;
          initServer(server,serverNode->name,SERVER_TYPE_FTP);
          server->ftp.loginName = String_duplicate(serverNode->ftp.loginName);
          Password_set(&server->ftp.password,&serverNode->ftp.password);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->writePreProcessCommand )
                                                               ? serverNode->writePreProcessCommand
                                                               : globalOptions.ftp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->writePostProcessCommand)
                                                               ? serverNode->writePostProcessCommand
                                                               : globalOptions.ftp.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find SSH server
        serverNode = LIST_FIND(&globalOptions.serverList,
                               serverNode,
                                  (serverNode->type == SERVER_TYPE_SSH)
                               && String_equals(serverNode->name,storageSpecifier->hostName)
                              );

        if (serverNode != NULL)
        {
          // get file server settings
          serverId  = serverNode->id;
          initServer(server,serverNode->name,SERVER_TYPE_SSH);
          server->ssh.loginName = String_duplicate(serverNode->ssh.loginName);
          server->ssh.port      = serverNode->ssh.port;
          Password_set(&server->ssh.password,&serverNode->ssh.password);
          Configuration_duplicateKey(&server->ssh.publicKey,&serverNode->ssh.publicKey);
          Configuration_duplicateKey(&server->ssh.privateKey,&serverNode->ssh.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->writePreProcessCommand )
                                                               ? serverNode->writePreProcessCommand
                                                               : globalOptions.scp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->writePostProcessCommand)
                                                              ? serverNode->writePostProcessCommand
                                                              : globalOptions.scp.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_SFTP:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find SSH server
        serverNode = LIST_FIND(&globalOptions.serverList,
                               serverNode,
                                  (serverNode->type == SERVER_TYPE_SSH)
                               && String_equals(serverNode->name,storageSpecifier->hostName)
                              );

        if (serverNode != NULL)
        {
          // get file server settings
          serverId  = serverNode->id;
          initServer(server,serverNode->name,SERVER_TYPE_SSH);
          server->ssh.loginName = String_duplicate(serverNode->ssh.loginName);
          server->ssh.port      = serverNode->ssh.port;
          Password_set(&server->ssh.password,&serverNode->ssh.password);
          Configuration_duplicateKey(&server->ssh.publicKey,&serverNode->ssh.publicKey);
          Configuration_duplicateKey(&server->ssh.privateKey,&serverNode->ssh.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->writePreProcessCommand )
                                                               ? serverNode->writePreProcessCommand
                                                               : globalOptions.sftp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->writePostProcessCommand)
                                                               ? serverNode->writePostProcessCommand
                                                               : globalOptions.sftp.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_WEBDAV:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find file server
        serverNode = LIST_FIND(&globalOptions.serverList,
                               serverNode,
                                  (serverNode->type == SERVER_TYPE_WEBDAV)
                               && String_equals(serverNode->name,storageSpecifier->hostName)
                              );

        if (serverNode != NULL)
        {
          // get WebDAV server settings
          serverId = serverNode->id;
          initServer(server,serverNode->name,SERVER_TYPE_WEBDAV);
          server->webDAV.loginName = String_duplicate(serverNode->webDAV.loginName);
          Password_set(&server->webDAV.password,&serverNode->webDAV.password);
          Configuration_duplicateKey(&server->webDAV.publicKey,&serverNode->webDAV.publicKey);
          Configuration_duplicateKey(&server->webDAV.privateKey,&serverNode->webDAV.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->writePreProcessCommand )
                                                               ? serverNode->writePreProcessCommand
                                                               : globalOptions.webdav.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->writePostProcessCommand)
                                                               ? serverNode->writePostProcessCommand
                                                               : globalOptions.webdav.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
    case STORAGE_TYPE_DEVICE:
      // nothing to do
      break;
    case STORAGE_TYPE_ANY:
      // nothing to do
      break;
    case STORAGE_TYPE_UNKNOWN:
      // nothing to do
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; // not reached
    #endif /* NDEBUG */
  }

  return serverId;
}

uint initFileServerSettings(FileServer       *fileServer,
                            ConstString      directory,
                            const JobOptions *jobOptions
                           )
{
  ServerNode *serverNode;

  assert(fileServer != NULL);
  assert(directory != NULL);

  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(fileServer);

  serverNode = NULL;
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    // find file server
    serverNode = LIST_FIND(&globalOptions.serverList,
                           serverNode,
                              (serverNode->type == SERVER_TYPE_FILE)
                           && String_startsWith(serverNode->name,directory)
                          );

    // get file server settings
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

void doneFileServerSettings(FileServer *fileServer)
{
  assert(fileServer != NULL);

  UNUSED_VARIABLE(fileServer);
}

uint initFTPServerSettings(FTPServer        *ftpServer,
                           ConstString      hostName,
                           const JobOptions *jobOptions
                          )
{
  ServerNode *serverNode;

  assert(ftpServer != NULL);
  assert(hostName != NULL);

  ftpServer->loginName = String_new();
  Password_init(&ftpServer->password);

  serverNode = NULL;
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    // find FTP server
    serverNode = LIST_FIND(&globalOptions.serverList,
                           serverNode,
                              (serverNode->type == SERVER_TYPE_FTP)
                           && String_equals(serverNode->name,hostName)
                          );

    // get FTP server settings
    String_set(ftpServer->loginName,
               ((jobOptions != NULL) && !String_isEmpty(jobOptions->ftpServer.loginName))
                 ? jobOptions->ftpServer.loginName
                 : ((serverNode != NULL)
                      ? serverNode->ftp.loginName
                      : globalOptions.defaultFTPServer.ftp.loginName
                   )
              );
    Password_set(&ftpServer->password,
                 ((jobOptions != NULL) && !Password_isEmpty(&jobOptions->ftpServer.password))
                   ? &jobOptions->ftpServer.password
                   : ((serverNode != NULL)
                      ? &serverNode->ftp.password
                      : &globalOptions.defaultFTPServer.ftp.password
                     )
                );
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

void doneFTPServerSettings(FTPServer *ftpServer)
{
  assert(ftpServer != NULL);

  Password_done(&ftpServer->password);
  String_delete(ftpServer->loginName);
}

uint initSSHServerSettings(SSHServer        *sshServer,
                           ConstString      hostName,
                           const JobOptions *jobOptions
                          )
{
  ServerNode *serverNode;

  assert(sshServer != NULL);
  assert(hostName != NULL);

  sshServer->loginName = String_new();
  Password_init(&sshServer->password);

  serverNode = NULL;
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    // find SSH server
    serverNode = LIST_FIND(&globalOptions.serverList,
                           serverNode,
                              (serverNode->type == SERVER_TYPE_SSH)
                           && String_equals(serverNode->name,hostName)
                          );

    // get SSH server settings
    sshServer->port       = ((jobOptions != NULL) && (jobOptions->sshServer.port != 0)                )
                              ? jobOptions->sshServer.port
                              : ((serverNode != NULL)
                                   ? serverNode->ssh.port
                                   : globalOptions.defaultSSHServer.ssh.port
                                );
    String_set(sshServer->loginName,
               ((jobOptions != NULL) && !String_isEmpty(jobOptions->sshServer.loginName) )
                 ? jobOptions->sshServer.loginName
                 : ((serverNode != NULL)
                      ? serverNode->ssh.loginName
                      : globalOptions.defaultSSHServer.ssh.loginName
                   )
              );
    Password_set(&sshServer->password,
                 ((jobOptions != NULL) && !Password_isEmpty(&jobOptions->sshServer.password))
                   ? &jobOptions->sshServer.password
                   : ((serverNode != NULL)
                        ? &serverNode->ssh.password
                        : &globalOptions.defaultSSHServer.ssh.password
                     )
                );
    Configuration_duplicateKey(&sshServer->publicKey,
                               ((jobOptions != NULL) && Configuration_isKeyAvailable(&jobOptions->sshServer.publicKey) )
                                 ? &jobOptions->sshServer.publicKey
                                 : ((serverNode != NULL)
                                      ? &serverNode->ssh.publicKey
                                      : &globalOptions.defaultSSHServer.ssh.publicKey
                                   )
                              );
    Configuration_duplicateKey(&sshServer->privateKey,
                               ((jobOptions != NULL) && Configuration_isKeyAvailable(&jobOptions->sshServer.privateKey))
                                 ? &jobOptions->sshServer.privateKey
                                 : ((serverNode != NULL)
                                      ? &serverNode->ssh.privateKey
                                      : &globalOptions.defaultSSHServer.ssh.privateKey
                                   )
                              );
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

void doneSSHServerSettings(SSHServer *sshServer)
{
  assert(sshServer != NULL);

  Configuration_doneKey(&sshServer->privateKey);
  Configuration_doneKey(&sshServer->publicKey);
  Password_done(&sshServer->password);
  String_delete(sshServer->loginName);
}

uint initWebDAVServerSettings(WebDAVServer     *webDAVServer,
                              ConstString      hostName,
                              const JobOptions *jobOptions
                             )
{
  ServerNode *serverNode;

  assert(hostName != NULL);
  assert(webDAVServer != NULL);

  webDAVServer->loginName = String_new();
  Password_init(&webDAVServer->password);

  serverNode = NULL;
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    // find WebDAV server
    serverNode = LIST_FIND(&globalOptions.serverList,
                           serverNode,
                              (serverNode->type == SERVER_TYPE_WEBDAV)
                           && String_equals(serverNode->name,hostName)
                          );

    // get WebDAV server settings
//    webDAVServer->port       = ((jobOptions != NULL) && (jobOptions->webDAVServer.port != 0) ? jobOptions->webDAVServer.port : ((serverNode != NULL) ? serverNode->webDAVServer.port : globalOptions.defaultWebDAVServer->port );
    String_set(webDAVServer->loginName,
               ((jobOptions != NULL) && !String_isEmpty(jobOptions->webDAVServer.loginName))
                 ? jobOptions->webDAVServer.loginName
                 : ((serverNode != NULL)
                      ? serverNode->webDAV.loginName
                      : globalOptions.defaultWebDAVServer.webDAV.loginName
                   )
              );
    Password_set(&webDAVServer->password,
                 ((jobOptions != NULL) && !Password_isEmpty(&jobOptions->webDAVServer.password))
                   ? &jobOptions->webDAVServer.password
                   : ((serverNode != NULL)
                        ? &serverNode->webDAV.password
                        : &globalOptions.defaultWebDAVServer.webDAV.password
                     )
                );
    Configuration_duplicateKey(&webDAVServer->publicKey,
                               ((jobOptions != NULL) && Configuration_isKeyAvailable(&jobOptions->webDAVServer.publicKey))
                                 ? &jobOptions->webDAVServer.publicKey
                                 : ((serverNode != NULL)
                                      ? &serverNode->webDAV.publicKey
                                      : &globalOptions.defaultWebDAVServer.webDAV.publicKey
                                   )
                              );
    Configuration_duplicateKey(&webDAVServer->privateKey,
                               ((jobOptions != NULL) && Configuration_isKeyAvailable(&jobOptions->webDAVServer.privateKey))
                                 ? &jobOptions->webDAVServer.privateKey
                                 : ((serverNode != NULL)
                                      ? &serverNode->webDAV.privateKey
                                      : &globalOptions.defaultWebDAVServer.webDAV.privateKey
                                   )
                              );
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

void doneWebDAVServerSettings(WebDAVServer *webDAVServer)
{
  assert(webDAVServer != NULL);

  Configuration_doneKey(&webDAVServer->privateKey);
  Configuration_doneKey(&webDAVServer->publicKey);
  Password_done(&webDAVServer->password);
  String_delete(webDAVServer->loginName);
}

void initCDSettings(OpticalDisk      *cd,
                    const JobOptions *jobOptions
                   )
{
  assert(cd != NULL);

  cd->deviceName              = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.deviceName             )) ? jobOptions->opticalDisk.deviceName              : globalOptions.cd.deviceName;
  cd->requestVolumeCommand    = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.requestVolumeCommand   )) ? jobOptions->opticalDisk.requestVolumeCommand    : globalOptions.cd.requestVolumeCommand;
  cd->unloadVolumeCommand     = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.unloadVolumeCommand    )) ? jobOptions->opticalDisk.unloadVolumeCommand     : globalOptions.cd.unloadVolumeCommand;
  cd->loadVolumeCommand       = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.loadVolumeCommand      )) ? jobOptions->opticalDisk.loadVolumeCommand       : globalOptions.cd.loadVolumeCommand;
  cd->volumeSize              = ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize != 0LL                     )) ? jobOptions->opticalDisk.volumeSize              : globalOptions.cd.volumeSize;
  cd->imagePreProcessCommand  = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imagePreProcessCommand )) ? jobOptions->opticalDisk.imagePreProcessCommand  : globalOptions.cd.imagePreProcessCommand;
  cd->imagePostProcessCommand = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imagePostProcessCommand)) ? jobOptions->opticalDisk.imagePostProcessCommand : globalOptions.cd.imagePostProcessCommand;
  cd->imageCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imageCommand           )) ? jobOptions->opticalDisk.imageCommand            : globalOptions.cd.imageCommand;
  cd->eccPreProcessCommand    = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccPreProcessCommand   )) ? jobOptions->opticalDisk.eccPreProcessCommand    : globalOptions.cd.eccPreProcessCommand;
  cd->eccPostProcessCommand   = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccPostProcessCommand  )) ? jobOptions->opticalDisk.eccPostProcessCommand   : globalOptions.cd.eccPostProcessCommand;
  cd->eccCommand              = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccCommand             )) ? jobOptions->opticalDisk.eccCommand              : globalOptions.cd.eccCommand;
  cd->blankCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccCommand             )) ? jobOptions->opticalDisk.blankCommand            : globalOptions.cd.blankCommand;
  cd->writePreProcessCommand  = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writePreProcessCommand )) ? jobOptions->opticalDisk.writePreProcessCommand  : globalOptions.cd.writePreProcessCommand;
  cd->writePostProcessCommand = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writePostProcessCommand)) ? jobOptions->opticalDisk.writePostProcessCommand : globalOptions.cd.writePostProcessCommand;
  cd->writeCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writeCommand           )) ? jobOptions->opticalDisk.writeCommand            : globalOptions.cd.writeCommand;
  cd->writeImageCommand       = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writeImageCommand      )) ? jobOptions->opticalDisk.writeImageCommand       : globalOptions.cd.writeImageCommand;
}

void initDVDSettings(OpticalDisk      *dvd,
                     const JobOptions *jobOptions
                    )
{
  assert(dvd != NULL);

  dvd->deviceName              = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.deviceName             )) ? jobOptions->opticalDisk.deviceName              : globalOptions.dvd.deviceName;
  dvd->requestVolumeCommand    = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.requestVolumeCommand   )) ? jobOptions->opticalDisk.requestVolumeCommand    : globalOptions.dvd.requestVolumeCommand;
  dvd->unloadVolumeCommand     = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.unloadVolumeCommand    )) ? jobOptions->opticalDisk.unloadVolumeCommand     : globalOptions.dvd.unloadVolumeCommand;
  dvd->loadVolumeCommand       = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.loadVolumeCommand      )) ? jobOptions->opticalDisk.loadVolumeCommand       : globalOptions.dvd.loadVolumeCommand;
  dvd->volumeSize              = ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize != 0LL                     )) ? jobOptions->opticalDisk.volumeSize              : globalOptions.dvd.volumeSize;
  dvd->imagePreProcessCommand  = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imagePreProcessCommand )) ? jobOptions->opticalDisk.imagePreProcessCommand  : globalOptions.dvd.imagePreProcessCommand;
  dvd->imagePostProcessCommand = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imagePostProcessCommand)) ? jobOptions->opticalDisk.imagePostProcessCommand : globalOptions.dvd.imagePostProcessCommand;
  dvd->imageCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imageCommand           )) ? jobOptions->opticalDisk.imageCommand            : globalOptions.dvd.imageCommand;
  dvd->eccPreProcessCommand    = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccPreProcessCommand   )) ? jobOptions->opticalDisk.eccPreProcessCommand    : globalOptions.dvd.eccPreProcessCommand;
  dvd->eccPostProcessCommand   = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccPostProcessCommand  )) ? jobOptions->opticalDisk.eccPostProcessCommand   : globalOptions.dvd.eccPostProcessCommand;
  dvd->eccCommand              = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccCommand             )) ? jobOptions->opticalDisk.eccCommand              : globalOptions.dvd.eccCommand;
  dvd->blankCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccCommand             )) ? jobOptions->opticalDisk.blankCommand            : globalOptions.dvd.blankCommand;
  dvd->writePreProcessCommand  = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writePreProcessCommand )) ? jobOptions->opticalDisk.writePreProcessCommand  : globalOptions.dvd.writePreProcessCommand;
  dvd->writePostProcessCommand = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writePostProcessCommand)) ? jobOptions->opticalDisk.writePostProcessCommand : globalOptions.dvd.writePostProcessCommand;
  dvd->writeCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writeCommand           )) ? jobOptions->opticalDisk.writeCommand            : globalOptions.dvd.writeCommand;
  dvd->writeImageCommand       = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writeImageCommand      )) ? jobOptions->opticalDisk.writeImageCommand       : globalOptions.dvd.writeImageCommand;
}

void initBDSettings(OpticalDisk      *bd,
                    const JobOptions *jobOptions
                   )
{
  assert(bd != NULL);

  bd->deviceName              = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.deviceName             )) ? jobOptions->opticalDisk.deviceName              : globalOptions.bd.deviceName;
  bd->requestVolumeCommand    = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.requestVolumeCommand   )) ? jobOptions->opticalDisk.requestVolumeCommand    : globalOptions.bd.requestVolumeCommand;
  bd->unloadVolumeCommand     = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.unloadVolumeCommand    )) ? jobOptions->opticalDisk.unloadVolumeCommand     : globalOptions.bd.unloadVolumeCommand;
  bd->loadVolumeCommand       = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.loadVolumeCommand      )) ? jobOptions->opticalDisk.loadVolumeCommand       : globalOptions.bd.loadVolumeCommand;
  bd->volumeSize              = ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize != 0LL                     )) ? jobOptions->opticalDisk.volumeSize              : globalOptions.bd.volumeSize;
  bd->imagePreProcessCommand  = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imagePreProcessCommand )) ? jobOptions->opticalDisk.imagePreProcessCommand  : globalOptions.bd.imagePreProcessCommand;
  bd->imagePostProcessCommand = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imagePostProcessCommand)) ? jobOptions->opticalDisk.imagePostProcessCommand : globalOptions.bd.imagePostProcessCommand;
  bd->imageCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.imageCommand           )) ? jobOptions->opticalDisk.imageCommand            : globalOptions.bd.imageCommand;
  bd->eccPreProcessCommand    = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccPreProcessCommand   )) ? jobOptions->opticalDisk.eccPreProcessCommand    : globalOptions.bd.eccPreProcessCommand;
  bd->eccPostProcessCommand   = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccPostProcessCommand  )) ? jobOptions->opticalDisk.eccPostProcessCommand   : globalOptions.bd.eccPostProcessCommand;
  bd->eccCommand              = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccCommand             )) ? jobOptions->opticalDisk.eccCommand              : globalOptions.bd.eccCommand;
  bd->blankCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.eccCommand             )) ? jobOptions->opticalDisk.blankCommand            : globalOptions.bd.blankCommand;
  bd->writePreProcessCommand  = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writePreProcessCommand )) ? jobOptions->opticalDisk.writePreProcessCommand  : globalOptions.bd.writePreProcessCommand;
  bd->writePostProcessCommand = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writePostProcessCommand)) ? jobOptions->opticalDisk.writePostProcessCommand : globalOptions.bd.writePostProcessCommand;
  bd->writeCommand            = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writeCommand           )) ? jobOptions->opticalDisk.writeCommand            : globalOptions.bd.writeCommand;
  bd->writeImageCommand       = ((jobOptions != NULL) && !String_isEmpty(jobOptions->opticalDisk.writeImageCommand      )) ? jobOptions->opticalDisk.writeImageCommand       : globalOptions.bd.writeImageCommand;
}

void doneOpticalDiskSettings(OpticalDisk *opticalDisk)
{
  assert(opticalDisk != NULL);

  UNUSED_VARIABLE(opticalDisk);
}

void initDeviceSettings(Device           *device,
                        ConstString      name,
                        const JobOptions *jobOptions
                       )
{
  DeviceNode *deviceNode;

  assert(device != NULL);
  assert(name != NULL);

  SEMAPHORE_LOCKED_DO(&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    // find device
    deviceNode = LIST_FIND(&globalOptions.deviceList,
                           deviceNode,
                           String_equals(deviceNode->device.name,name)
                          );

    // get device settings
    device->name                    = ((jobOptions != NULL) && (jobOptions->device.name                    != NULL)) ? jobOptions->device.name                    : ((deviceNode != NULL) ? deviceNode->device.name                    : globalOptions.defaultDevice.name                   );
    device->requestVolumeCommand    = ((jobOptions != NULL) && (jobOptions->device.requestVolumeCommand    != NULL)) ? jobOptions->device.requestVolumeCommand    : ((deviceNode != NULL) ? deviceNode->device.requestVolumeCommand    : globalOptions.defaultDevice.requestVolumeCommand   );
    device->unloadVolumeCommand     = ((jobOptions != NULL) && (jobOptions->device.unloadVolumeCommand     != NULL)) ? jobOptions->device.unloadVolumeCommand     : ((deviceNode != NULL) ? deviceNode->device.unloadVolumeCommand     : globalOptions.defaultDevice.unloadVolumeCommand    );
    device->loadVolumeCommand       = ((jobOptions != NULL) && (jobOptions->device.loadVolumeCommand       != NULL)) ? jobOptions->device.loadVolumeCommand       : ((deviceNode != NULL) ? deviceNode->device.loadVolumeCommand       : globalOptions.defaultDevice.loadVolumeCommand      );
    device->volumeSize              = ((jobOptions != NULL) && (jobOptions->device.volumeSize              != 0LL )) ? jobOptions->device.volumeSize              : ((deviceNode != NULL) ? deviceNode->device.volumeSize              : globalOptions.defaultDevice.volumeSize             );
    device->imagePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->device.imagePreProcessCommand  != NULL)) ? jobOptions->device.imagePreProcessCommand  : ((deviceNode != NULL) ? deviceNode->device.imagePreProcessCommand  : globalOptions.defaultDevice.imagePreProcessCommand );
    device->imagePostProcessCommand = ((jobOptions != NULL) && (jobOptions->device.imagePostProcessCommand != NULL)) ? jobOptions->device.imagePostProcessCommand : ((deviceNode != NULL) ? deviceNode->device.imagePostProcessCommand : globalOptions.defaultDevice.imagePostProcessCommand);
    device->imageCommand            = ((jobOptions != NULL) && (jobOptions->device.imageCommand            != NULL)) ? jobOptions->device.imageCommand            : ((deviceNode != NULL) ? deviceNode->device.imageCommand            : globalOptions.defaultDevice.imageCommand           );
    device->eccPreProcessCommand    = ((jobOptions != NULL) && (jobOptions->device.eccPreProcessCommand    != NULL)) ? jobOptions->device.eccPreProcessCommand    : ((deviceNode != NULL) ? deviceNode->device.eccPreProcessCommand    : globalOptions.defaultDevice.eccPreProcessCommand   );
    device->eccPostProcessCommand   = ((jobOptions != NULL) && (jobOptions->device.eccPostProcessCommand   != NULL)) ? jobOptions->device.eccPostProcessCommand   : ((deviceNode != NULL) ? deviceNode->device.eccPostProcessCommand   : globalOptions.defaultDevice.eccPostProcessCommand  );
    device->eccCommand              = ((jobOptions != NULL) && (jobOptions->device.eccCommand              != NULL)) ? jobOptions->device.eccCommand              : ((deviceNode != NULL) ? deviceNode->device.eccCommand              : globalOptions.defaultDevice.eccCommand             );
    device->blankCommand            = ((jobOptions != NULL) && (jobOptions->device.eccCommand              != NULL)) ? jobOptions->device.eccCommand              : ((deviceNode != NULL) ? deviceNode->device.blankCommand            : globalOptions.defaultDevice.blankCommand           );
    device->writePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->device.writePreProcessCommand  != NULL)) ? jobOptions->device.writePreProcessCommand  : ((deviceNode != NULL) ? deviceNode->device.writePreProcessCommand  : globalOptions.defaultDevice.writePreProcessCommand );
    device->writePostProcessCommand = ((jobOptions != NULL) && (jobOptions->device.writePostProcessCommand != NULL)) ? jobOptions->device.writePostProcessCommand : ((deviceNode != NULL) ? deviceNode->device.writePostProcessCommand : globalOptions.defaultDevice.writePostProcessCommand);
    device->writeCommand            = ((jobOptions != NULL) && (jobOptions->device.writeCommand            != NULL)) ? jobOptions->device.writeCommand            : ((deviceNode != NULL) ? deviceNode->device.writeCommand            : globalOptions.defaultDevice.writeCommand           );
  }
}

void doneDeviceSettings(Device *device)
{
  assert(device != NULL);

  UNUSED_VARIABLE(device);
}

bool Configuration_parseWeekDaySet(const char *names, WeekDaySet *weekDaySet)
{
  StringTokenizer stringTokenizer;
  ConstString     token;

  assert(names != NULL);
  assert(weekDaySet != NULL);

  if (stringEquals(names,"*"))
  {
    (*weekDaySet) = WEEKDAY_SET_ANY;
  }
  else
  {
    SET_CLEAR(*weekDaySet);

    String_initTokenizerCString(&stringTokenizer,
                                names,
                                ",",
                                STRING_QUOTES,
                                TRUE
                               );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      if      (String_equalsIgnoreCaseCString(token,"mon")) SET_ADD(*weekDaySet,WEEKDAY_MON);
      else if (String_equalsIgnoreCaseCString(token,"tue")) SET_ADD(*weekDaySet,WEEKDAY_TUE);
      else if (String_equalsIgnoreCaseCString(token,"wed")) SET_ADD(*weekDaySet,WEEKDAY_WED);
      else if (String_equalsIgnoreCaseCString(token,"thu")) SET_ADD(*weekDaySet,WEEKDAY_THU);
      else if (String_equalsIgnoreCaseCString(token,"fri")) SET_ADD(*weekDaySet,WEEKDAY_FRI);
      else if (String_equalsIgnoreCaseCString(token,"sat")) SET_ADD(*weekDaySet,WEEKDAY_SAT);
      else if (String_equalsIgnoreCaseCString(token,"sun")) SET_ADD(*weekDaySet,WEEKDAY_SUN);
      else
      {
        String_doneTokenizer(&stringTokenizer);
        return FALSE;
      }
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return TRUE;
}

bool Configuration_parseDateNumber(ConstString s, int *n)
{
  ulong i;
  long  nextIndex;

  assert(s != NULL);
  assert(n != NULL);

  // init variables
  if   (String_equalsCString(s,"*"))
  {
    (*n) = DATE_ANY;
  }
  else
  {
    i = STRING_BEGIN;
    if (String_length(s) > 0)
    {
      while ((i < String_length(s)-1) && (String_index(s,i) == '0'))
      {
        i++;
      }
    }
    (*n) = (int)String_toInteger(s,i,&nextIndex,NULL,0);
    if (nextIndex != STRING_END) return FALSE;
  }

  return TRUE;
}

bool Configuration_parseDateMonth(ConstString s, int *month)
{
  String name;
  ulong i;
  long   nextIndex;

  assert(s != NULL);
  assert(month != NULL);

  name = String_toLower(String_duplicate(s));
  if      (String_equalsCString(s,"*"))
  {
    (*month) = DATE_ANY;
  }
  else if (String_equalsIgnoreCaseCString(name,"jan")) (*month) = MONTH_JAN;
  else if (String_equalsIgnoreCaseCString(name,"feb")) (*month) = MONTH_FEB;
  else if (String_equalsIgnoreCaseCString(name,"mar")) (*month) = MONTH_MAR;
  else if (String_equalsIgnoreCaseCString(name,"apr")) (*month) = MONTH_APR;
  else if (String_equalsIgnoreCaseCString(name,"may")) (*month) = MONTH_MAY;
  else if (String_equalsIgnoreCaseCString(name,"jun")) (*month) = MONTH_JUN;
  else if (String_equalsIgnoreCaseCString(name,"jul")) (*month) = MONTH_JUL;
  else if (String_equalsIgnoreCaseCString(name,"aug")) (*month) = MONTH_AUG;
  else if (String_equalsIgnoreCaseCString(name,"sep")) (*month) = MONTH_SEP;
  else if (String_equalsIgnoreCaseCString(name,"oct")) (*month) = MONTH_OCT;
  else if (String_equalsIgnoreCaseCString(name,"nov")) (*month) = MONTH_NOV;
  else if (String_equalsIgnoreCaseCString(name,"dec")) (*month) = MONTH_DEC;
  else
  {
    i = STRING_BEGIN;
    if (String_length(s) > 0)
    {
      while ((i < String_length(s)-1) && (String_index(s,i) == '0'))
      {
        i++;
      }
    }
    (*month) = (uint)String_toInteger(s,i,&nextIndex,NULL,0);
    if ((nextIndex != STRING_END) || ((*month) < 1) || ((*month) > 12))
    {
      String_delete(name);
      return FALSE;
    }
  }
  String_delete(name);

  return TRUE;
}

bool Configuration_parseTimeNumber(ConstString s, int *n)
{
  ulong i;
  long  nextIndex;

  assert(s != NULL);
  assert(n != NULL);

  // init variables
  if   (String_equalsCString(s,"*"))
  {
    (*n) = TIME_ANY;
  }
  else
  {
    i = STRING_BEGIN;
    if (String_length(s) > 0)
    {
      while ((i < String_length(s)-1) && (String_index(s,i) == '0'))
      {
        i++;
      }
    }
    (*n) = (int)String_toInteger(s,i,&nextIndex,NULL,0);
    if (nextIndex != STRING_END) return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : parseBandWidth
* Purpose: parse band width
* Input  : s                - band width string
*          errorMessage     - error message
*          errorMessageSize - max. error message size
* Output : -
* Return : band width node or NULL
* Notes  : -
\***********************************************************************/

LOCAL BandWidthNode *parseBandWidth(ConstString s, char errorMessage[], uint errorMessageSize)
{
  BandWidthNode *bandWidthNode;
  bool          errorFlag;
  String        s0,s1,s2;
  long          nextIndex;

  assert(s != NULL);

  // allocate new bandwidth node
  bandWidthNode = newBandWidthNode();

  // parse bandwidth. Format: (<band width>[K|M])|<file name> <date> [<weekday>] <time>
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  nextIndex = STRING_BEGIN;
  if      (   String_matchCString(s,nextIndex,"^\\s*[[:digit:]]+\\s*$",&nextIndex,s0,NULL)
           || String_matchCString(s,nextIndex,"^\\s*[[:digit:]]+\\S+",&nextIndex,s0,NULL)
          )
  {
    // value
    if (!parseBandWidthNumber(s0,&bandWidthNode->n))
    {
      if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth value '%s'",String_cString(s0));
      errorFlag = TRUE;
    }
    bandWidthNode->fileName = NULL;
  }
  else if (String_parse(s,nextIndex,"%S",&nextIndex,s0))
  {
    // external file
    bandWidthNode->n        = 0L;
    bandWidthNode->fileName = String_duplicate(s0);
  }
  else
  {
    stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth value '%s'",s);
    errorFlag = TRUE;
  }
  if (nextIndex < (long)String_length(s))
  {
    if      (String_parse(s,nextIndex,"%S-%S-%S",&nextIndex,s0,s1,s2))
    {
      if (!Configuration_parseDateNumber(s0,&bandWidthNode->year ))
      {
        if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth year '%s'",String_cString(s0));
        errorFlag = TRUE;
      }
      if (!Configuration_parseDateMonth     (s1,&bandWidthNode->month))
      {
        if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth month '%s'",String_cString(s1));
        errorFlag = TRUE;
      }
      if (!Configuration_parseTimeNumber(s2,&bandWidthNode->day  ))
      {
        if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth day '%s'",String_cString(s2));
        errorFlag = TRUE;
      }
    }
    else
    {
      stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth date in '%s'",String_cString(s));
      errorFlag = TRUE;
    }
    if (nextIndex < (long)String_length(s))
    {
      if      (String_parse(s,nextIndex,"%S %S:%S",&nextIndex,s0,s1,s2))
      {
        if (!Configuration_parseWeekDaySet(String_cString(s0),&bandWidthNode->weekDaySet))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth weekday '%s'",String_cString(s0));
          errorFlag = TRUE;
        }
        if (!Configuration_parseTimeNumber(s1,&bandWidthNode->hour  ))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth hour '%s'",String_cString(s1));
          errorFlag = TRUE;
        }
        if (!Configuration_parseTimeNumber(s2,&bandWidthNode->minute))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth minute '%s'",String_cString(s2));
          errorFlag = TRUE;
        }
      }
      else if (String_parse(s,nextIndex,"%S:%S",&nextIndex,s0,s1))
      {
        if (!Configuration_parseTimeNumber(s0,&bandWidthNode->hour  ))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth hour '%s'",String_cString(s0));
          errorFlag = TRUE;
        }
        if (!Configuration_parseTimeNumber(s1,&bandWidthNode->minute))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth minute '%s'",String_cString(s1));
          errorFlag = TRUE;
        }
      }
      else
      {
        if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth weekday/time in '%s'",s);
        errorFlag = TRUE;
      }
    }
  }
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);

  if (errorFlag || (nextIndex < (long)String_length(s)))
  {
    deleteBandWidthNode(bandWidthNode);
    return NULL;
  }

  return bandWidthNode;
}

/***********************************************************************\
* Name   : cmdOptionParseBandWidth
* Purpose: command line option call back for parsing band width settings
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseBandWidth(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  BandWidthNode *bandWidthNode;
  String        s;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  // parse band width node
  s = String_newCString(value);
  bandWidthNode = parseBandWidth(s,errorMessage,errorMessageSize);
  if (bandWidthNode == NULL)
  {
    String_delete(s);
    return FALSE;
  }
  String_delete(s);

  // append to list
  List_append((BandWidthList*)variable,bandWidthNode);

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseOwner
* Purpose: command line option call back for parsing owner
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseOwner(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  char   userName[256],groupName[256];
  uint32 userId,groupId;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  // parse
  if      (String_scanCString(value,"%256s:%256s",userName,groupName))
  {
    userId  = Misc_userNameToUserId(userName);
    groupId = Misc_groupNameToGroupId(groupName);
  }
  else if (String_scanCString(value,"%256s:",userName))
  {
    userId  = Misc_userNameToUserId(userName);
    groupId = FILE_DEFAULT_GROUP_ID;
  }
  else if (String_scanCString(value,":%256s",groupName))
  {
    userId  = FILE_DEFAULT_USER_ID;
    groupId = Misc_groupNameToGroupId(groupName);
  }
  else if (String_scanCString(value,"%256s",userName))
  {
    userId  = Misc_userNameToUserId(userName);
    groupId = FILE_DEFAULT_GROUP_ID;
  }
  else
  {
    stringFormat(errorMessage,errorMessageSize,"Unknown owner ship value '%s'",value);
    return FALSE;
  }

  // store owner values
  ((Owner*)variable)->userId  = userId;
  ((Owner*)variable)->groupId = groupId;

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParsePermissions
* Purpose: command line option call back for parsing permissions
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParsePermissions(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  char           user[4],group[4],world[4];
  uint           n;
  FilePermission permission;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  // parse
  permission = FILE_PERMISSION_NONE;
  if      (String_scanCString(value,"%o",permission))
  {
    permission = (FilePermission)atol(value);
  }
  else if (String_scanCString(value,"%4s:%4s:%4s",user,group,world))
  {
    n = stringLength(user);
    if      ((n >= 1) && (toupper(user[0]) == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1]) == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2]) == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
    n = stringLength(group);
    if      ((n >= 1) && (toupper(group[0]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
    else if ((n >= 2) && (toupper(group[1]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
    else if ((n >= 3) && (toupper(group[2]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
    n = stringLength(world);
    if      ((n >= 1) && (toupper(world[0]) == 'R')) permission |= FILE_PERMISSION_OTHER_READ;
    else if ((n >= 2) && (toupper(world[1]) == 'W')) permission |= FILE_PERMISSION_OTHER_WRITE;
    else if ((n >= 3) && (toupper(world[2]) == 'X')) permission |= FILE_PERMISSION_OTHER_EXECUTE;
  }
  else if (String_scanCString(value,"%4s:%4s",user,group))
  {
    n = stringLength(user);
    if      ((n >= 1) && (toupper(user[0]) == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1]) == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2]) == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
    n = stringLength(group);
    if      ((n >= 1) && (toupper(group[0]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
    else if ((n >= 2) && (toupper(group[1]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
    else if ((n >= 3) && (toupper(group[2]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
  }
  else if (String_scanCString(value,"%4s",user))
  {
    n = stringLength(user);
    if      ((n >= 1) && (toupper(user[0]) == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1]) == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2]) == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
  }
  else
  {
    stringFormat(errorMessage,errorMessageSize,"Unknown permissions value '%s'",value);
    return FALSE;
  }

  // store owner values
  (*((FilePermission*)variable)) = permission;

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParsePassword
* Purpose: command line option call back for parsing password
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  Password_setCString((Password*)variable,value);

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseHashData
* Purpose: command line option call back for parsing password to hash
*          data
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseHashData(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  CryptHash cryptHash;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // calculate hash
  Crypt_initHash(&cryptHash,PASSWORD_HASH_ALGORITHM);
  Crypt_updateHash(&cryptHash,value,stringLength(value));

  // store hash
  Configuration_setHash((Hash*)variable,&cryptHash);

  // free resources
  Crypt_doneHash(&cryptHash);

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionReadCertificateFile
* Purpose: command line option call back for reading certificate file
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionReadCertificateFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  Certificate *certificate = (Certificate*)variable;
  Errors      error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  error = Configuration_readCertificateFile(certificate,value);
  if (error != ERROR_NONE)
  {
    stringSet(errorMessage,errorMessageSize,Error_getText(error));
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionReadKeyFile
* Purpose: command line option call back for reading key file
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionReadKeyFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  Key    *key = (Key*)variable;
  Errors error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  error = Configuration_readKeyFile(key,value);
  if (error != ERROR_NONE)
  {
    stringSet(errorMessage,errorMessageSize,Error_getText(error));
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseKey
* Purpose: command line option call back for get key data
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseKey(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  Key    *key = (Key*)variable;
  Errors error;
  uint   dataLength;
  void   *data;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  if (File_existsCString(value))
  {
    // read key data from file
    error = Configuration_readKeyFile(key,value);
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,errorMessageSize,Error_getText(error));
      return FALSE;
    }
  }
  else if (stringStartsWith(value,"base64:"))
  {
    // decode base64 encoded key data

    // get key data length
    dataLength = Misc_base64DecodeLengthCString(&value[7]);
    if (dataLength > 0)
    {
      // allocate key memory
      data = allocSecure(dataLength);
      if (data == NULL)
      {
        stringSet(errorMessage,errorMessageSize,"insufficient secure memory");
        return FALSE;
      }

      // decode base64
      if (!Misc_base64DecodeCString((byte*)data,dataLength,NULL,&value[7]))
      {
        stringSet(errorMessage,errorMessageSize,"decode base64 fail");
        freeSecure(data);
        return FALSE;
      }

      // set key data
      if (key->data != NULL) freeSecure(key->data);
      key->data   = data;
      key->length = dataLength;
    }
  }
  else
  {
    // get plain key data

    // get key data length
    dataLength = stringLength(value);
    if (dataLength > 0)
    {
      // allocate key memory
      data = allocSecure(dataLength);
      if (data == NULL)
      {
        stringSet(errorMessage,errorMessageSize,"insufficient secure memory");
        return FALSE;
      }

      // copy data
      memCopyFast(data,dataLength,value,dataLength);

      // set key data
      if (key->data != NULL) freeSecure(key->data);
      key->data   = data;
      key->length = dataLength;
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseArchiveFileModeOverwrite
* Purpose: command line option call back for archive file mode overwrite
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseArchiveFileModeOverwrite(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if      (   (value == NULL)
           || stringEquals(value,"1")
           || stringEqualsIgnoreCase(value,"true")
           || stringEqualsIgnoreCase(value,"on")
           || stringEqualsIgnoreCase(value,"yes")
          )
  {
    (*(ArchiveFileModes*)variable) = ARCHIVE_FILE_MODE_OVERWRITE;
  }
  else
  {
    (*(ArchiveFileModes*)variable) = ARCHIVE_FILE_MODE_STOP;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseRestoreEntryModeOverwrite
* Purpose: command line option call back for restore entry mode
*          overwrite
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseRestoreEntryModeOverwrite(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if      (   (value == NULL)
           || stringEquals(value,"1")
           || stringEqualsIgnoreCase(value,"true")
           || stringEqualsIgnoreCase(value,"on")
           || stringEqualsIgnoreCase(value,"yes")
          )
  {
    (*(RestoreEntryModes*)variable) = RESTORE_ENTRY_MODE_OVERWRITE;
  }
  else
  {
    (*(RestoreEntryModes*)variable) = RESTORE_ENTRY_MODE_STOP;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseStorageFlagNoStorage
* Purpose: command line option call back for storage flag no-storage
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseStorageFlagNoStorage(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((StorageFlags*)variable)->noStorage = TRUE;

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseStorageFlagDryRun
* Purpose: command line option call back for storage flag no-storage
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseStorageFlagDryRun(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((StorageFlags*)variable)->dryRun = TRUE;

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseDeprecatedMountDevice
* Purpose: command line option call back for archive files overwrite mode
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseDeprecatedMountDevice(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (!stringIsEmpty(value))
  {
    // add to mount list
    mountNode = newMountNodeCString(value,
                                    NULL  // deviceName
                                   );
    assert(mountNode != NULL);
    List_append((MountList*)variable,mountNode);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseDeprecatedStopOnError
* Purpose: command line option call back for deprecated option
*          stop-on-error
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseDeprecatedStopOnError(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (value != NULL)
  {
    (*(bool*)variable) = !(   (stringEqualsIgnoreCase(value,"1") == 0)
                           || (stringEqualsIgnoreCase(value,"true") == 0)
                           || (stringEqualsIgnoreCase(value,"on") == 0)
                           || (stringEqualsIgnoreCase(value,"yes") == 0)
                          );
  }
  else
  {
    (*(bool*)variable) = FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseConfigFile
* Purpose: command line option call back for parsing configuration
*          filename
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueConfigFileParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String         string;
  ConfigFileNode *configFileNode;

  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

//TODO: required?
  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  // add to config list
  Configuration_add(CONFIG_FILE_TYPE_CONFIG,String_cString(string));

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueFormatMaintenanceDate
* Purpose: format maintenance config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueConfigFileFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = ((ConfigFileList*)data)->head;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<file name>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ConfigFileNode *configFileNode = (const ConfigFileNode*)(*formatUserData);
        String               line            = (String)data;

        while (   (configFileNode != NULL)
               && (configFileNode->type != CONFIG_FILE_TYPE_CONFIG)
              )
        {
          configFileNode = configFileNode->next;
        }

        if (configFileNode != NULL)
        {
          String_formatAppend(line,"%S",configFileNode->fileName);

          (*formatUserData) = configFileNode->next;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueMaintenanceDateParse
* Purpose: config value option call back for parsing maintenance date
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

LOCAL bool configValueMaintenanceDateParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  bool         errorFlag;
  String       s0,s1,s2;
//TODO
  ScheduleDate date;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  if      (String_parseCString(value,"%S-%S-%S",NULL,s0,s1,s2))
  {
    if (!Configuration_parseDateNumber(s0,&date.year )) errorFlag = TRUE;
    if (!Configuration_parseDateMonth (s1,&date.month)) errorFlag = TRUE;
    if (!Configuration_parseDateNumber(s2,&date.day  )) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);
  if (errorFlag)
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse maintenance date '%s'",value);
    return FALSE;
  }

  // store values
  (*(ScheduleDate*)variable) = date;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueFormatMaintenanceDate
* Purpose: format maintenance config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueMaintenanceDateFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (MaintenanceDate*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<yyyy>|*-<mm>|*-<dd>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ScheduleDate *scheduleDate = (const ScheduleDate*)(*formatUserData);
        String             line          = (String)data;

        if (scheduleDate != NULL)
        {
          if (scheduleDate->year != DATE_ANY)
          {
            String_appendFormat(line,"%d",scheduleDate->year);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,'-');
          if (scheduleDate->month != DATE_ANY)
          {
            String_appendFormat(line,"%d",scheduleDate->month);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,'-');
          if (scheduleDate->day != DATE_ANY)
          {
            String_appendFormat(line,"%d",scheduleDate->day);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueMaintenanceWeekDaySetParse
* Purpose: config value option call back for parsing maintenance week
*          day set
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

LOCAL bool configValueMaintenanceWeekDaySetParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  WeekDaySet weekDaySet;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!Configuration_parseWeekDaySet(value,&weekDaySet))
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse maintenance weekday '%s'",value);
    return FALSE;
  }

  // store value
  (*(WeekDaySet*)variable) = weekDaySet;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueMaintenanceWeekDaySetFormat
* Purpose: format maintenance config week day set
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueMaintenanceWeekDaySetFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (WeekDaySet*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"Mon|Tue|Wed|Thu|Fri|Sat|Sun|*,...");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ScheduleWeekDaySet *scheduleWeekDaySet = (ScheduleWeekDaySet*)(*formatUserData);
        String                   names;
        String                   line                = (String)data;

        if (scheduleWeekDaySet != NULL)
        {
          if ((*scheduleWeekDaySet) != WEEKDAY_SET_ANY)
          {
            names = String_new();

            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_MON)) { String_joinCString(names,"Mon",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_TUE)) { String_joinCString(names,"Tue",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_WED)) { String_joinCString(names,"Wed",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_THU)) { String_joinCString(names,"Thu",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_FRI)) { String_joinCString(names,"Fri",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_SAT)) { String_joinCString(names,"Sat",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_SUN)) { String_joinCString(names,"Sun",','); }

            String_append(line,names);
            String_appendChar(line,' ');

            String_delete(names);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseMaintenanceTime
* Purpose: config value option call back for parsing maintenance time
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

LOCAL bool configValueMaintenanceTimeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  bool         errorFlag;
  String       s0,s1;
  ScheduleTime time;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  if (String_parseCString(value,"%S:%S",NULL,s0,s1))
  {
    if (!Configuration_parseTimeNumber(s0,&time.hour  )) errorFlag = TRUE;
    if (!Configuration_parseTimeNumber(s1,&time.minute)) errorFlag = TRUE;
  }
  String_delete(s1);
  String_delete(s0);
  if (errorFlag)
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse maintenance time '%s'",value);
    return FALSE;
  }

  // store values
  (*(ScheduleTime*)variable) = time;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueFormatMaintenanceTime
* Purpose: format maintenance config
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueMaintenanceTimeFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (MaintenanceTime*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<hh>|*:<mm>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const MaintenanceTime *maintenanceTime = (const MaintenanceTime*)(*formatUserData);
        String                line             = (String)data;

        if (maintenanceTime != NULL)
        {
          if (maintenanceTime->hour != TIME_ANY)
          {
            String_appendFormat(line,"%d",maintenanceTime->hour);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,':');
          if (maintenanceTime->minute != TIME_ANY)
          {
            String_appendFormat(line,"%d",maintenanceTime->minute);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueServerSectionIteratorNext
* Purpose: get next server node
* Input  : sectionDataIterator - section data iterator
*          serverType          - server type
* Output : next section data or NULL
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *configValueServerSectionIteratorNext(ConfigValueSectionDataIterator *sectionDataIterator, ServerTypes serverType)
{
  ServerNode *serverNode = (ServerNode*)(*sectionDataIterator);

  assert(sectionDataIterator != NULL);

  while ((serverNode != NULL) && (serverNode->type != serverType))
  {
    serverNode = serverNode->next;
  }

  if (serverNode != NULL)
  {
    (*sectionDataIterator) = serverNode->next;
  }

  return serverNode;
}

/***********************************************************************\
* Name   : configValueServerFTPSectionDataIteratorNext
* Purpose: get next FTP server node
* Input  : sectionDataIterator - section data iterator
*          userData            - user data
* Output : next section data or NULL
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *configValueServerFTPSectionDataIteratorNext(ConfigValueSectionDataIterator *sectionDataIterator, void *userData)
{
  assert(sectionDataIterator != NULL);

  UNUSED_VARIABLE(userData);

  return configValueServerSectionIteratorNext(sectionDataIterator,SERVER_TYPE_FTP);
}

/***********************************************************************\
* Name   : configValueServerSSHSectionDataIteratorNext
* Purpose: get next SSH server node
* Input  : sectionDataIterator - section data iterator
*          userData            - user data
* Output : next section data or NULL
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *configValueServerSSHSectionDataIteratorNext(ConfigValueSectionDataIterator *sectionDataIterator, void *userData)
{
  assert(sectionDataIterator != NULL);

  UNUSED_VARIABLE(userData);

  return configValueServerSectionIteratorNext(sectionDataIterator,SERVER_TYPE_SSH);
}

/***********************************************************************\
* Name   : configValueServerWebDAVSectionIteratorNext
* Purpose: get next WebDAV server node
* Input  : sectionDataIterator - section data iterator
*          userData            - user data
* Output : next section data or NULL
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *configValueServerWebDAVSectionDataIteratorNext(ConfigValueSectionDataIterator *sectionDataIterator, void *userData)
{
  assert(sectionDataIterator != NULL);

  UNUSED_VARIABLE(userData);

  return configValueServerSectionIteratorNext(sectionDataIterator,SERVER_TYPE_WEBDAV);
}

/***********************************************************************\
* Name   : configValueDeprecatedMountDeviceParse
* Purpose: config value option call back for deprecated mount-device
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

LOCAL bool configValueDeprecatedMountDeviceParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String    string;
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  if (!stringIsEmpty(value))
  {
    // add to mount list
    mountNode = newMountNodeCString(value,
                                    NULL  // deviceName
                                   );
    if (mountNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    List_append((MountList*)variable,mountNode);
  }

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedStopOnErrorParse
* Purpose: config value option call back for deprecated stop-on-error
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

LOCAL bool configValueDeprecatedStopOnErrorParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (value != NULL)
  {
    (*(bool*)variable) = !(   (stringEqualsIgnoreCase(value,"1") == 0)
                           || (stringEqualsIgnoreCase(value,"true") == 0)
                           || (stringEqualsIgnoreCase(value,"on") == 0)
                           || (stringEqualsIgnoreCase(value,"yes") == 0)
                          );
  }
  else
  {
    (*(bool*)variable) = FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedArchiveFileModeOverwriteParse
* Purpose: config value option call back for deprecated overwrite-files
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

LOCAL bool configValueDeprecatedArchiveFileModeOverwriteParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(ArchiveFileModes*)variable) = ARCHIVE_FILE_MODE_OVERWRITE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedRestoreEntryModeOverwriteParse
* Purpose: config value option call back for deprecated overwrite-files
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

LOCAL bool configValueDeprecatedRestoreEntryModeOverwriteParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(RestoreEntryModes*)variable) = RESTORE_ENTRY_MODE_OVERWRITE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleDateParse
* Purpose: config value option call back for parsing schedule date
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

LOCAL bool configValueScheduleDateParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  bool         errorFlag;
  String       s0,s1,s2;
  ScheduleDate date;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  if      (String_parseCString(value,"%S-%S-%S",NULL,s0,s1,s2))
  {
    if (!Configuration_parseDateNumber(s0,&date.year )) errorFlag = TRUE;
    if (!Configuration_parseDateMonth (s1,&date.month)) errorFlag = TRUE;
    if (!Configuration_parseDateNumber(s2,&date.day  )) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);
  if (errorFlag)
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse schedule date '%s'",value);
    return FALSE;
  }

  // store values
  (*(ScheduleDate*)variable) = date;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleDateFormat
* Purpose: format schedule config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleDateFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (ScheduleDate*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<yyyy>|*-<mm>|*-<dd>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ScheduleDate *scheduleDate = (const ScheduleDate*)(*formatUserData);
        String             line          = (String)data;

        if (scheduleDate != NULL)
        {
          if (scheduleDate->year != DATE_ANY)
          {
            String_appendFormat(line,"%4d",scheduleDate->year);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,'-');
          if (scheduleDate->month != DATE_ANY)
          {
            String_appendFormat(line,"%02d",scheduleDate->month);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,'-');
          if (scheduleDate->day != DATE_ANY)
          {
            String_appendFormat(line,"%02d",scheduleDate->day);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleWeekDaySetParse
* Purpose: config value option call back for parsing schedule week day
*          set
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

LOCAL bool configValueScheduleWeekDaySetParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  WeekDaySet weekDaySet;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!Configuration_parseWeekDaySet(value,&weekDaySet))
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse schedule weekday '%s'",value);
    return FALSE;
  }

  // store value
  (*(WeekDaySet*)variable) = weekDaySet;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleWeekDaySetFormat
* Purpose: format schedule config week day set
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleWeekDaySetFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (ScheduleWeekDaySet*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"Mon|Tue|Wed|Thu|Fri|Sat|Sun|*,...");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ScheduleWeekDaySet *scheduleWeekDaySet = (const ScheduleWeekDaySet*)(*formatUserData);
        String                   names;
        String                   line                = (String)data;

        if (scheduleWeekDaySet != NULL)
        {
          if ((*scheduleWeekDaySet) != WEEKDAY_SET_ANY)
          {
            names = String_new();

            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_MON)) { String_joinCString(names,"Mon",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_TUE)) { String_joinCString(names,"Tue",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_WED)) { String_joinCString(names,"Wed",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_THU)) { String_joinCString(names,"Thu",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_FRI)) { String_joinCString(names,"Fri",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_SAT)) { String_joinCString(names,"Sat",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_SUN)) { String_joinCString(names,"Sun",','); }

            String_append(line,names);
            String_appendChar(line,' ');

            String_delete(names);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleTimeParse
* Purpose: config value option call back for parsing schedule time
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

LOCAL bool configValueScheduleTimeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  bool         errorFlag;
  String       s0,s1;
  ScheduleTime time;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  if (String_parseCString(value,"%S:%S",NULL,s0,s1))
  {
    if (!Configuration_parseTimeNumber(s0,&time.hour  )) errorFlag = TRUE;
    if (!Configuration_parseTimeNumber(s1,&time.minute)) errorFlag = TRUE;
  }
  String_delete(s1);
  String_delete(s0);
  if (errorFlag)
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse schedule time '%s'",value);
    return FALSE;
  }

  // store values
  (*(ScheduleTime*)variable) = time;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleTimeFormat
* Purpose: format schedule config
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleTimeFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (ScheduleTime*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<hh>|*:<mm>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ScheduleTime *scheduleTime = (ScheduleTime*)(*formatUserData);
        String             line          = (String)data;

        if (scheduleTime != NULL)
        {
          if (scheduleTime->hour != TIME_ANY)
          {
            String_appendFormat(line,"%02d",scheduleTime->hour);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,':');
          if (scheduleTime->minute != TIME_ANY)
          {
            String_appendFormat(line,"%02d",scheduleTime->minute);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMinKeepParse
* Purpose: config value option call back for parsing min. keep
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

LOCAL bool configValuePersistenceMinKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  int minKeep;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!stringEquals(value,"*"))
  {
    if (!String_parseCString(value,"%d",NULL,&minKeep))
    {
      snprintf(errorMessage,errorMessageSize,"Cannot parse persistence min. keep '%s'",value);
      return FALSE;
    }
  }
  else
  {
    minKeep = KEEP_ALL;
  }

  // store values
  (*(int*)variable) = minKeep;

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMinKeepFormat
* Purpose: format min. keep config
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMinKeepFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (PersistenceNode*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<n>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const PersistenceNode *persistenceNode = (const PersistenceNode*)(*formatUserData);
        String                line             = (String)data;

        if (persistenceNode != NULL)
        {
          if (persistenceNode->minKeep != KEEP_ALL)
          {
            String_appendFormat(line,"%d",persistenceNode->minKeep);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMaxKeepParse
* Purpose: config value option call back for parsing max. keep
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

LOCAL bool configValuePersistenceMaxKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  int maxKeep;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!stringEquals(value,"*"))
  {
    if (!String_parseCString(value,"%d",NULL,&maxKeep))
    {
      snprintf(errorMessage,errorMessageSize,"Cannot parse persistence max. keep '%s'",value);
      return FALSE;
    }
  }
  else
  {
    maxKeep = KEEP_ALL;
  }

  // store values
  (*(int*)variable) = maxKeep;

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMaxKeepFormat
* Purpose: format max. keep config
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMaxKeepFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (PersistenceNode*)data;

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (PersistenceNode*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<n>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const PersistenceNode *persistenceNode = (const PersistenceNode*)(*formatUserData);
        String                line             = (String)data;

        if (persistenceNode != NULL)
        {
          if (persistenceNode->maxKeep != KEEP_ALL)
          {
            String_appendFormat(line,"%d",persistenceNode->maxKeep);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParsePersistenceMaxAge
* Purpose: config value option call back for parsing max. age
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

LOCAL bool configValuePersistenceMaxAgeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  int maxAge;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!stringEquals(value,"*"))
  {
    if (!String_parseCString(value,"%d",NULL,&maxAge))
    {
      snprintf(errorMessage,errorMessageSize,"Cannot parse persistence max. age '%s'",value);
      return FALSE;
    }
  }
  else
  {
    maxAge = AGE_FOREVER;
  }

  // store values
  (*(int*)variable) = maxAge;

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMaxAgeFormat
* Purpose: format max. age config
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMaxAgeFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (PersistenceNode*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<n>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const PersistenceNode *persistenceNode = (const PersistenceNode*)(*formatUserData);
        String                line             = (String)data;

        if (persistenceNode != NULL)
        {
          if (persistenceNode->maxAge != AGE_FOREVER)
          {
            String_appendFormat(line,"%d",persistenceNode->maxAge);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedRemoteHostParse
* Purpose: config value option call back for deprecated remote host
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

bool configValueDeprecatedRemoteHostParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String string;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  String_set(*((String*)variable),string);

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedRemotePortParse
* Purpose: config value option call back for deprecated remote port
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

bool configValueDeprecatedRemotePortParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  uint n;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  if (!stringToUInt(value,&n))
  {
    stringFormat(errorMessage,errorMessageSize,"expected port number: 0..65535");
    return FALSE;
  }
  (*(uint*)variable) = n;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedScheduleMinKeepParse
* Purpose: config value option call back for deprecated min. keep of
*          schedule
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

LOCAL bool configValueDeprecatedScheduleMinKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((ScheduleNode*)variable)->minKeep                   = strtol(value,NULL,0);
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedScheduleMaxKeepParse
* Purpose: config value option call back for deprecated max. keep of
*          schedule
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

LOCAL bool configValueDeprecatedScheduleMaxKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((ScheduleNode*)variable)->maxKeep                   = strtol(value,NULL,0);
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedScheduleMaxAgeParse
* Purpose: config value option call back for deprecated max. age of
*          schedule
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

LOCAL bool configValueDeprecatedScheduleMaxAgeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((ScheduleNode*)variable)->maxAge                    = strtol(value,NULL,0);
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;

  return TRUE;
}

/***********************************************************************\
* Name   : freeBandWidthNode
* Purpose: free band width node
* Input  : bandWidthNode - band width node
*          userData      - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeBandWidthNode(BandWidthNode *bandWidthNode, void *userData)
{
  assert(bandWidthNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(bandWidthNode->fileName);
}

/***********************************************************************\
* Name   : validateOptions
* Purpose: validate options
* Input  : -
* Output : -
* Return : TRUE if options valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool validateOptions(void)
{
  if (!String_isEmpty(globalOptions.tmpDirectory))
  {
    if (!File_exists(globalOptions.tmpDirectory)) { printError(_("Temporary directory '%s' does not exists!"),String_cString(globalOptions.tmpDirectory)); return FALSE; }
    if (!File_isDirectory(globalOptions.tmpDirectory)) { printError(_("'%s' is not a directory!"),String_cString(globalOptions.tmpDirectory)); return FALSE; }
    if (!File_isWritable(globalOptions.tmpDirectory)) { printError(_("Temporary directory '%s' is not writable!"),String_cString(globalOptions.tmpDirectory)); return FALSE; }
  }

  if (!Continuous_isAvailable())
  {
    printWarning("Continuous support is not available");
  }

  return TRUE;
}

/*---------------------------------------------------------------------*/

String getConfigFileName(String fileName)
{
  const ConfigFileNode *configFileNode;

  assert(fileName != NULL);

  String_clear(fileName);

  configFileNode = LIST_FIND_LAST(&configFileList,configFileNode,File_isWritable(configFileNode->fileName));
  if (configFileNode != NULL)
  {
    String_set(fileName,configFileNode->fileName);
  }

  return fileName;
}

/*---------------------------------------------------------------------*/

ulong getBandWidth(BandWidthList *bandWidthList)
{
  uint          currentYear,currentMonth,currentDay;
  WeekDays      currentWeekDay;
  uint          currentHour,currentMinute;
  BandWidthNode *matchingBandWidthNode;
  bool          dateMatchFlag,weekDayMatchFlag,timeMatchFlag;
  BandWidthNode *bandWidthNode;
  ulong         n;
  uint64        timestamp;
  FileHandle    fileHandle;
  String        line;

  assert(bandWidthList != NULL);

  n = 0L;

  // get current date/time values
  Misc_splitDateTime(Misc_getCurrentDateTime(),
                     &currentYear,
                     &currentMonth,
                     &currentDay,
                     &currentHour,
                     &currentMinute,
                     NULL,
                     &currentWeekDay
                    );

  // find best matching band width node
  matchingBandWidthNode = NULL;
  LIST_ITERATE(bandWidthList,bandWidthNode)
  {

    // match date
    dateMatchFlag =       (matchingBandWidthNode == NULL)
                       || (   (   (bandWidthNode->year == DATE_ANY)
                               || (   (currentYear >= (uint)bandWidthNode->year)
                                   && (   (matchingBandWidthNode->year == DATE_ANY)
                                       || (bandWidthNode->year > matchingBandWidthNode->year)
                                      )
                                  )
                              )
                           && (   (bandWidthNode->month == DATE_ANY)
                               || (   (bandWidthNode->year == DATE_ANY)
                                   || (currentYear > (uint)bandWidthNode->year)
                                   || (   (currentMonth >= (uint)bandWidthNode->month)
                                       && (   (matchingBandWidthNode->month == DATE_ANY)
                                           || (bandWidthNode->month > matchingBandWidthNode->month)
                                          )
                                      )
                                  )
                              )
                           && (   (bandWidthNode->day    == DATE_ANY)
                               || (   (bandWidthNode->month == DATE_ANY)
                                   || (currentMonth > (uint)bandWidthNode->month)
                                   || (   (currentDay >= (uint)bandWidthNode->day)
                                       && (   (matchingBandWidthNode->day == DATE_ANY)
                                           || (bandWidthNode->day > matchingBandWidthNode->day)
                                          )
                                      )
                                  )
                              )
                          );

    // check week day
    weekDayMatchFlag =    (matchingBandWidthNode == NULL)
                       || (   (bandWidthNode->weekDaySet == WEEKDAY_SET_ANY)
                           && IN_SET(bandWidthNode->weekDaySet,currentWeekDay)
                          );

    // check time
    timeMatchFlag =    (matchingBandWidthNode == NULL)
                    || (   (   (bandWidthNode->hour  == TIME_ANY)
                            || (   (currentHour >= (uint)bandWidthNode->hour)
                                && (   (matchingBandWidthNode->hour == TIME_ANY)
                                    || (bandWidthNode->hour > matchingBandWidthNode->hour)
                                    )
                               )
                           )
                        && (   (bandWidthNode->minute == TIME_ANY)
                            || (   (bandWidthNode->hour == TIME_ANY)
                                || (currentHour > (uint)bandWidthNode->hour)
                                || (   (currentMinute >= (uint)bandWidthNode->minute)
                                    && (   (matchingBandWidthNode->minute == TIME_ANY)
                                        || (bandWidthNode->minute > matchingBandWidthNode->minute)
                                       )
                                   )
                               )
                           )
                       );

    // check if matching band width node found
    if (dateMatchFlag && weekDayMatchFlag && timeMatchFlag)
    {
      matchingBandWidthNode = bandWidthNode;
    }
  }

  if (matchingBandWidthNode != NULL)
  {
    // read band width
    if (matchingBandWidthNode->fileName != NULL)
    {
      // read from external file
      timestamp = Misc_getTimestamp();
      if (timestamp > (bandWidthList->lastReadTimestamp+5*US_PER_SECOND))
      {
        bandWidthList->n = 0LL;

        // open file
        if (File_open(&fileHandle,matchingBandWidthNode->fileName,FILE_OPEN_READ) == ERROR_NONE)
        {
          line = String_new();
          while (File_getLine(&fileHandle,line,NULL,"#;"))
          {
            // parse band width
            if (!parseBandWidthNumber(line,&bandWidthList->n))
            {
              continue;
            }
          }
          String_delete(line);

          // close file
          (void)File_close(&fileHandle);

          // store timestamp of last read
          bandWidthList->lastReadTimestamp = timestamp;
        }
      }

      n = bandWidthList->n;
    }
    else
    {
      // use value
      n = matchingBandWidthNode->n;
    }
  }

  return n;
}

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

LOCAL bool configValuePasswordParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String string;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  // store password
  Password_setString((Password*)variable,string);

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePasswordFormat
* Purpose: format passwrd config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePasswordFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (Password*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<password>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const Password *password = (Password*)(*formatUserData);
        String         line      = (String)data;

        if (password != NULL)
        {
          PASSWORD_DEPLOY_DO(plainPassword,password)
          {
            String_appendFormat(line,"%'s",plainPassword);
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

#if 0
//TODO: remove
bool configValuePasswordHashParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if ((*(Hash**)variable) == NULL)
  {
    (*(Hash**)variable) = Crypt_newHash(PASSWORD_HASH_ALGORITHM);
  }
//  Password_setCString((Password*)variable,value);

  return TRUE;
}

LOCAL void configValueInitPassordHashFormat(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (*(Hash**)variable);
}

LOCAL void configValueDonePasswordHashFormat(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

LOCAL bool configValuePasswordHashFormat(void **formatUserData, void *userData, String line)
{
  Hash *passwordHash;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  passwordHash = (Hash*)(*formatUserData);
  if (passwordHash != NULL)
  {
    if (isHashsAvaiable(passwordHash))
    {
      String_appendFormat(line,"%s:",Crypt_hashAlgorithmToString(passwordHash->cryptHashAlgorithm,"plain"));
      Misc_base64Encode(line,passwordHash->data,passwordHash->length);
    }

    (*formatUserData) = NULL;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}
#endif

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

LOCAL bool configValueCryptAlgorithmsParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
#ifdef MULTI_CRYPT
  StringTokenizer stringTokenizer;
  ConstString     token;
  CryptAlgorithms cryptAlgorithms[4];
  uint            i;
#else /* not MULTI_CRYPT */
  CryptAlgorithms cryptAlgorithm;
#endif /* MULTI_CRYPT */

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

#ifdef MULTI_CRYPT
  i = 0;
  String_initTokenizerCString(&stringTokenizer,
                              value,
                              "+,",
                              STRING_QUOTES,
                              TRUE
                             );
  while (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    if (i >= 4)
    {
      stringFormat(errorMessage,errorMessageSize,"Too many crypt algorithms (max. 4)");
      String_doneTokenizer(&stringTokenizer);
      return FALSE;
    }

    if (!Crypt_parseAlgorithm(String_cString(token),&((JobOptionsCompressAlgorithm*)variable)[i]->cryptAlgorithm))
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown crypt algorithm '%s'",String_cString(token));
      String_doneTokenizer(&stringTokenizer);
      return FALSE;
    }
    ((JobOptionsCompressAlgorithm*)variable)[cryptAlgorithmCount]->cryptAlgorithmSet = TRUE;

    if (((JobOptionsCompressAlgorithm*)variable)[i]->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
    {
      i++;
    }
  }
  String_doneTokenizer(&stringTokenizer);
  while (i < 4)
  {
    ((JobOptionsCryptAlgorithms*)variable)->values[i] = CRYPT_ALGORITHM_NONE;
    i++;
  }
#else /* not MULTI_CRYPT */
  if (!Crypt_parseAlgorithm(value,&cryptAlgorithm))
  {
    stringFormat(errorMessage,errorMessageSize,"Unknown crypt algorithm '%s'",value);
    return FALSE;
  }

  ((CryptAlgorithms*)variable)[0] = cryptAlgorithm;
#endif /* MULTI_CRYPT */

  return TRUE;
}

/***********************************************************************\
* Name   : configValueCryptAlgorithmsFormat
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

LOCAL bool configValueCryptAlgorithmsFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
#ifdef MULTI_CRYPT
  uint            i;
#endif /* MULTI_CRYPT */

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (CryptAlgorithms*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<crypt algorithm>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const CryptAlgorithms *cryptAlgorithms = (CryptAlgorithms*)(*formatUserData);
        String                line             = (String)data;

        if (cryptAlgorithms != NULL)
        {
#ifdef MULTI_CRYPT
          i = 0;
          while ((i < 4) && (cryptAlgorithms[i] != CRYPT_ALGORITHM_NONE))
          {
            if (i > 0) String_appendChar(line,'+');
            String_appendCString(line,Crypt_algorithmToString(cryptAlgorithms[i],NULL));
            i++;
          }
#else /* not MULTI_CRYPT */
          String_appendCString(line,Crypt_algorithmToString(cryptAlgorithms[0],NULL));
#endif /* MULTI_CRYPT */

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValueBandWidthParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  BandWidthNode *bandWidthNode;
  String        s;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse band width node
  s = String_newCString(value);
  bandWidthNode = parseBandWidth(s,errorMessage,errorMessageSize);
  if (bandWidthNode == NULL)
  {
    String_delete(s);
    return FALSE;
  }
  String_delete(s);

  // append to list
  List_append((BandWidthList*)variable,bandWidthNode);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueBandWidthFormat
* Purpose: format next config band width setting
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

LOCAL bool configValueBandWidthFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  const StringUnit UNITS[] =
  {
    {"T",1024LL*1024LL*1024LL*1024LL},
    {"G",1024LL*1024LL*1024LL},
    {"M",1024LL*1024LL},
    {"K",1024LL},
  };

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = ((BandWidthList*)data)->head;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<band width>[K|M])|<file name> <date> [<weekday>] <time>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const BandWidthNode *bandWidthNode = (BandWidthNode*)(*formatUserData);
        StringUnit          stringUnit;
        String              line           = (String)data;

        if (bandWidthNode != NULL)
        {
          if (bandWidthNode->fileName != NULL)
          {
            String_appendFormat(line,"%s",bandWidthNode->fileName);
          }
          else
          {
            stringUnit = String_getMatchingUnit(bandWidthNode->n,UNITS,SIZE_OF_ARRAY(UNITS));
            String_appendFormat(line,"%lu%s",bandWidthNode->n/stringUnit.factor,stringUnit.name);
          }
          String_appendChar(line,' ');

          if (bandWidthNode->year != DATE_ANY)
          {
            String_appendFormat(line,"%04u",bandWidthNode->year);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,'-');
          if (bandWidthNode->month != DATE_ANY)
          {
            String_appendFormat(line,"%02u",bandWidthNode->month);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,'-');
          if (bandWidthNode->day != DATE_ANY)
          {
            String_appendFormat(line,"%02u",bandWidthNode->day);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,' ');

          if (bandWidthNode->hour != TIME_ANY)
          {
            String_appendFormat(line,"%02u",bandWidthNode->hour);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,':');
          if (bandWidthNode->minute != TIME_ANY)
          {
            String_appendFormat(line,"%02u",bandWidthNode->minute);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = bandWidthNode->next;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValueOwnerParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  char   userName[256],groupName[256];
  uint32 userId,groupId;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if      (String_scanCString(value,"%256s:%256s",userName,groupName))
  {
    userId  = Misc_userNameToUserId(userName);
    groupId = Misc_groupNameToGroupId(groupName);
  }
  else if (String_scanCString(value,"%256s:",userName))
  {
    userId  = Misc_userNameToUserId(userName);
    groupId = FILE_DEFAULT_GROUP_ID;
  }
  else if (String_scanCString(value,":%256s",groupName))
  {
    userId  = FILE_DEFAULT_USER_ID;
    groupId = Misc_groupNameToGroupId(groupName);
  }
  else if (String_scanCString(value,"%256s",userName))
  {
    userId  = Misc_userNameToUserId(userName);
    groupId = FILE_DEFAULT_GROUP_ID;
  }
  else
  {
    stringFormat(errorMessage,errorMessageSize,"Unknown owner ship value '%s'",value);
    return FALSE;
  }

  // store owner values
  ((Owner*)variable)->userId  = userId;
  ((Owner*)variable)->groupId = groupId;

  return TRUE;
}

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

LOCAL bool configValueOwnerFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (Owner*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<user>:<group>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const Owner *owner = (Owner*)(*formatUserData);
        String      line   = (String)data;
        char        userName[256],groupName[256];

        if (owner != NULL)
        {
          if (Misc_userIdToUserName  (userName, sizeof(userName), owner->userId )) return FALSE;
          if (Misc_groupIdToGroupName(groupName,sizeof(groupName),owner->groupId)) return FALSE;
          String_appendFormat(line,"%s:%s",userName,groupName);

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValuePermissionsParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  char           user[4],group[4],world[4];
  uint           n;
  FilePermission permission;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  permission = FILE_PERMISSION_NONE;
  if      (String_scanCString(value,"%o",permission))
  {
    permission = (FilePermission)atol(value);
  }
  else if (String_scanCString(value,"%4s:%4s:%4s",user,group,world))
  {
    n = stringLength(user);
    if      ((n >= 1) && (toupper(user[0])  == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1])  == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2])  == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
    n = stringLength(group);
    if      ((n >= 1) && (toupper(group[0]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
    else if ((n >= 2) && (toupper(group[1]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
    else if ((n >= 3) && (toupper(group[2]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
    n = stringLength(world);
    if      ((n >= 1) && (toupper(world[0]) == 'R')) permission |= FILE_PERMISSION_OTHER_READ;
    else if ((n >= 2) && (toupper(world[1]) == 'W')) permission |= FILE_PERMISSION_OTHER_WRITE;
    else if ((n >= 3) && (toupper(world[2]) == 'X')) permission |= FILE_PERMISSION_OTHER_EXECUTE;
  }
  else if (String_scanCString(value,"%4s:%4s",user,group))
  {
    n = stringLength(user);
    if      ((n >= 1) && (toupper(user[0])  == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1])  == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2])  == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
    n = stringLength(group);
    if      ((n >= 1) && (toupper(group[0]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
    else if ((n >= 2) && (toupper(group[1]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
    else if ((n >= 3) && (toupper(group[2]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
  }
  else if (String_scanCString(value,"%4s",user))
  {
    n = stringLength(user);
    if      ((n >= 1) && (toupper(user[0])  == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1])  == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2])  == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
  }
  else
  {
    stringFormat(errorMessage,errorMessageSize,"Unknown permissions value '%s'",value);
    return FALSE;
  }

  // store owner values
  (*((FilePermission*)variable)) = permission;

  return TRUE;
}

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

LOCAL bool configValuePermissionsFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  ;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (FilePermission*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<user>:<group>:<world>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const FilePermission *filePermission = (FilePermission*)(*formatUserData);
        String               line            = (String)data;

        if (filePermission != NULL)
        {
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_USER_READ ) ? 'r' : '-');
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_USER_READ ) ? 'r' : '-');
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_USER_READ ) ? 'r' : '-');
          String_appendChar(line,':');
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_GROUP_READ) ? 'r' : '-');
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_GROUP_READ) ? 'r' : '-');
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_GROUP_READ) ? 'r' : '-');
          String_appendChar(line,':');
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_OTHER_READ) ? 'r' : '-');
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_OTHER_READ) ? 'r' : '-');
          String_appendChar(line,IS_SET(*filePermission,FILE_PERMISSION_OTHER_READ) ? 'r' : '-');

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

LOCAL bool configValueEntryPatternParse(EntryTypes entryType, void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  const char* FILENAME_MAP_FROM[] = {"\\n","\\r","\\\\"};
  const char* FILENAME_MAP_TO[]   = {"\n","\r","\\"};

  PatternTypes patternType;
  String       string;
  Errors       error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(name);

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = (PatternTypes)userData;                  }

  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  // append to list
  String_mapCString(string,STRING_BEGIN,FILENAME_MAP_FROM,FILENAME_MAP_TO,SIZE_OF_ARRAY(FILENAME_MAP_FROM));
  error = EntryList_append((EntryList*)variable,entryType,string,patternType,NULL);
  if (error != ERROR_NONE)
  {
    stringSet(errorMessage,errorMessageSize,Error_getText(error));
    String_delete(string);
    return FALSE;
  }

  // free resources
  String_delete(string);

  return TRUE;
}

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

LOCAL bool configValueFileEntryPatternParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  return configValueEntryPatternParse(ENTRY_TYPE_FILE,userData,variable,name,value,errorMessage,errorMessageSize);
}

LOCAL bool configValueImageEntryPatternParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  return configValueEntryPatternParse(ENTRY_TYPE_IMAGE,userData,variable,name,value,errorMessage,errorMessageSize);
}

/***********************************************************************\
* Name   : configValueFileEntryPatternFormat,
*          configValueImageEntryPatternFormat
* Purpose: format next config include statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

LOCAL bool configValueFileEntryPatternFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  const char* FILENAME_MAP_FROM[] = {"\n","\r","\\"};
  const char* FILENAME_MAP_TO[]   = {"\\n","\\r","\\\\"};

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = ((EntryList*)data)->head;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"[r|x|g:]<pattern>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const EntryNode *entryNode = (EntryNode*)(*formatUserData);
        String          fileName;
        String          line       = (String)data;

        while ((entryNode != NULL) && (entryNode->type != ENTRY_TYPE_FILE))
        {
          entryNode = entryNode->next;
        }
        if (entryNode != NULL)
        {
          fileName = String_mapCString(String_duplicate(entryNode->string),STRING_BEGIN,FILENAME_MAP_FROM,FILENAME_MAP_TO,SIZE_OF_ARRAY(FILENAME_MAP_FROM));
          switch (entryNode->pattern.type)
          {
            case PATTERN_TYPE_GLOB:
              String_appendFormat(line,"%'S",fileName);
              break;
            case PATTERN_TYPE_REGEX:
              String_appendFormat(line,"r:%'S",fileName);
              break;
            case PATTERN_TYPE_EXTENDED_REGEX:
              String_appendFormat(line,"x:%'S",fileName);
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }
          String_delete(fileName);

          (*formatUserData) = entryNode->next;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

LOCAL bool configValueImageEntryPatternFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = ((EntryList*)data)->head;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"[r|x|g:]<pattern>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const EntryNode *entryNode = (EntryNode*)(*formatUserData);
        String          line       = (String)data;

        while ((entryNode != NULL) && (entryNode->type != ENTRY_TYPE_IMAGE))
        {
          entryNode = entryNode->next;
        }
        if (entryNode != NULL)
        {
          switch (entryNode->pattern.type)
          {
            case PATTERN_TYPE_GLOB:
              String_appendFormat(line,"%'S",entryNode->string);
              break;
            case PATTERN_TYPE_REGEX:
              String_appendFormat(line,"r:%'S",entryNode->string);
              break;
            case PATTERN_TYPE_EXTENDED_REGEX:
              String_appendFormat(line,"x:%'S",entryNode->string);
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }

          (*formatUserData) = entryNode->next;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValuePatternParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  PatternTypes patternType;
  String       string;
  Errors       error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  // append to pattern list
  error = PatternList_append((PatternList*)variable,string,patternType,NULL);
  if (error != ERROR_NONE)
  {
    stringSet(errorMessage,errorMessageSize,Error_getText(error));
    String_delete(string);
    return FALSE;
  }

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePatternFormat
* Purpose: format next config pattern statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePatternFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = ((PatternList*)data)->head;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"[r|x|g:]<pattern>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const PatternNode *patternNode = (PatternNode*)(*formatUserData);
        String            line         = (String)data;

        if (patternNode != NULL)
        {
          switch (patternNode->pattern.type)
          {
            case PATTERN_TYPE_GLOB:
              String_appendFormat(line,"%'S",patternNode->string);
              break;
            case PATTERN_TYPE_REGEX:
              String_appendFormat(line,"r:%'S",patternNode->string);
              break;
            case PATTERN_TYPE_EXTENDED_REGEX:
              String_appendFormat(line,"x:%'S",patternNode->string);
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }

          (*formatUserData) = patternNode->next;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValueMountParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String    mountName;
  String    deviceName;
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // init variables
  mountName  = String_new();
  deviceName = String_new();

  // get name, device
  if      (String_parseCString(value,"%S,%S,%y",NULL,mountName,deviceName,NULL))
  {
    // Note: %y deprecated
  }
  else if (String_parseCString(value,"%S,,%y",NULL,mountName,NULL))
  {
    // Note: %y deprecated
    String_clear(deviceName);
  }
  else if (String_parseCString(value,"%S,%y",NULL,mountName,NULL))
  {
    // Note: %y deprecated
    String_clear(deviceName);
  }
  else if (String_parseCString(value,"%S,%S",NULL,mountName,deviceName))
  {
  }
  else if (String_parseCString(value,"%S",NULL,mountName))
  {
    String_clear(deviceName);
  }
  else
  {
    String_delete(deviceName);
    String_delete(mountName);
    return FALSE;
  }

  if (!String_isEmpty(mountName))
  {
    // add to mount list
    mountNode = newMountNode(mountName,
                             deviceName
                            );
    if (mountNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    List_append((MountList*)variable,mountNode);
  }

  // free resources
  String_delete(deviceName);
  String_delete(mountName);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueMountFormat
* Purpose: format next config mount statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

LOCAL bool configValueMountFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = ((PatternList*)data)->head;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<mount point>,<device name>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const MountNode *mountNode = (MountNode*)(*formatUserData);
        String         line        = (String)data;

        if (mountNode != NULL)
        {
          String_appendFormat(line,"%'S",mountNode->name);
          if (!String_isEmpty(mountNode->device)) String_appendFormat(line,",%'S",mountNode->device);

          (*formatUserData) = mountNode->next;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValueDeltaSourceParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  PatternTypes    patternType;
  String          string;
  DeltaSourceNode *deltaSourceNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  // append to delta source list
  deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
  if (deltaSourceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  deltaSourceNode->storageName = String_duplicate(string);
  deltaSourceNode->patternType = patternType;
  List_append((DeltaSourceList*)variable,deltaSourceNode);

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeltaSourceFormat
* Purpose: format next config delta source pattern statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

LOCAL bool configValueDeltaSourceFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = ((PatternList*)data)->head;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"[r|x|g:]<name|pattern>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const DeltaSourceNode *deltaSourceNode = (const DeltaSourceNode*)(*formatUserData);
        String                line             = (String)data;

        if (deltaSourceNode != NULL)
        {
          switch (deltaSourceNode->patternType)
          {
            case PATTERN_TYPE_GLOB:
              String_appendFormat(line,"%'S",deltaSourceNode->storageName);
              break;
            case PATTERN_TYPE_REGEX:
              String_appendFormat(line,"r:%'S",deltaSourceNode->storageName);
              break;
            case PATTERN_TYPE_EXTENDED_REGEX:
              String_appendFormat(line,"x:%'S",deltaSourceNode->storageName);
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }

          (*formatUserData) = deltaSourceNode->next;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValueCompressAlgorithmsParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  char                          algorithm1[256],algorithm2[256];
  CompressAlgorithms            compressAlgorithmDelta,compressAlgorithmByte;
  bool                          foundFlag;
  const CommandLineOptionSelect *select;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  compressAlgorithmDelta = COMPRESS_ALGORITHM_NONE;
  compressAlgorithmByte  = COMPRESS_ALGORITHM_NONE;

  // parse
  if (   String_scanCString(value,"%256s+%256s",algorithm1,algorithm2)
      || String_scanCString(value,"%256s,%256s",algorithm1,algorithm2)
     )
  {
    foundFlag = FALSE;
    for (select = COMPRESS_ALGORITHMS_DELTA; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(algorithm1,select->name))
      {
        if ((CompressAlgorithms)select->value != COMPRESS_ALGORITHM_NONE)
        {
          compressAlgorithmDelta = (CompressAlgorithms)select->value;
        }
        foundFlag = TRUE;
        break;
      }
    }
    for (select = COMPRESS_ALGORITHMS_BYTE; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(algorithm1,select->name))
      {
        if ((CompressAlgorithms)select->value != COMPRESS_ALGORITHM_NONE)
        {
          compressAlgorithmByte = (CompressAlgorithms)select->value;
        }
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag)
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",algorithm1);
      return FALSE;
    }

    foundFlag = FALSE;
    for (select = COMPRESS_ALGORITHMS_DELTA; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(algorithm2,select->name))
      {
        if ((CompressAlgorithms)select->value != COMPRESS_ALGORITHM_NONE)
        {
          compressAlgorithmDelta = (CompressAlgorithms)select->value;
        }
        foundFlag = TRUE;
        break;
      }
    }
    for (select = COMPRESS_ALGORITHMS_BYTE; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(algorithm2,select->name))
      {
        if ((CompressAlgorithms)select->value != COMPRESS_ALGORITHM_NONE)
        {
          compressAlgorithmByte = (CompressAlgorithms)select->value;
        }
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag)
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",algorithm2);
      return FALSE;
    }
  }
  else
  {
    foundFlag = FALSE;
    for (select = COMPRESS_ALGORITHMS_DELTA; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(value,select->name))
      {
        if ((CompressAlgorithms)select->value != COMPRESS_ALGORITHM_NONE)
        {
          compressAlgorithmDelta = (CompressAlgorithms)select->value;
        }
        foundFlag = TRUE;
        break;
      }
    }
    for (select = COMPRESS_ALGORITHMS_BYTE; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(value,select->name))
      {
        if ((CompressAlgorithms)select->value != COMPRESS_ALGORITHM_NONE)
        {
          compressAlgorithmByte = (CompressAlgorithms)select->value;
        }
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag)
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",value);
      return FALSE;
    }
  }

  // store compress algorithm values
  ((CompressAlgorithmsDeltaByte*)variable)->delta = compressAlgorithmDelta;
  ((CompressAlgorithmsDeltaByte*)variable)->byte  = compressAlgorithmByte;

  return TRUE;
}

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

LOCAL bool configValueCompressAlgorithmsFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (CompressAlgorithmsDeltaByte*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<delta compression>+<byte compression>|<delta compression>|<byte compression>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        CompressAlgorithmsDeltaByte *compressAlgorithmsDeltaByte = (CompressAlgorithmsDeltaByte*)(*formatUserData);
        String                      line                         = (String)data;

        if (compressAlgorithmsDeltaByte != NULL)
        {
          String_appendFormat(line,
                              "%s+%s",
                              CmdOption_selectToString(COMPRESS_ALGORITHMS_DELTA,compressAlgorithmsDeltaByte->delta,NULL),
                              CmdOption_selectToString(COMPRESS_ALGORITHMS_BYTE, compressAlgorithmsDeltaByte->byte, NULL)
                             );

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValueCertificateParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  Certificate *certificate = (Certificate*)variable;
  Errors      error;
  uint        dataLength;
  void        *data;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if      (File_existsCString(value))
  {
    // read certificate from file
    error = Configuration_readCertificateFile(certificate,value);
    if (error != ERROR_NONE)
    {
      return FALSE;
    }
    certificate->type = CERTIFICATE_TYPE_FILE;
    String_setCString(certificate->fileName,value);
  }
  else if (stringStartsWith(value,"base64:"))
  {
    // get certificate from inline base64 data

    // get certificate data length
    dataLength = Misc_base64DecodeLengthCString(&value[7]);

    if (dataLength > 0)
    {
      // allocate certificate memory
      data = malloc(dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // decode base64
      if (!Misc_base64DecodeCString((byte*)data,dataLength,NULL,&value[7]))
      {
        freeSecure(data);
        return FALSE;
      }
    }
    else
    {
      data = NULL;
    }

    // set certificate data
    if (certificate->data != NULL) free(certificate->data);
    certificate->type   = CERTIFICATE_TYPE_CONFIG;
    certificate->data   = data;
    certificate->length = dataLength;
  }
  else
  {
    // get certificate from inline data

    // get certificate data length
    dataLength = stringLength(value);

    if (dataLength > 0)
    {
      // allocate certificate memory
      data = malloc(dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // copy data
      memCopyFast(data,dataLength,value,dataLength);
    }
    else
    {
      data = NULL;
    }

    // set certificate data
    if (certificate->data != NULL) free(certificate->data);
    certificate->data   = data;
    certificate->length = dataLength;
  }

  return TRUE;
}

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

LOCAL bool configValueCertificateFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (CompressAlgorithmsDeltaByte*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<file name>|base64:<data>|<data>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const Certificate *certificate = (Certificate*)(*formatUserData);
        String            line         = (String)data;

        if (certificate != NULL)
        {
          if (Configuration_isCertificateAvailable(certificate))
          {
            switch (certificate->type)
            {
              case CERTIFICATE_TYPE_FILE:
                String_appendFormat(line,"%S",certificate->fileName);
                break;
              case CERTIFICATE_TYPE_CONFIG:
                String_appendCString(line,"base64:");
                Misc_base64Encode(line,certificate->data,certificate->length);
                break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break;
            }
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

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

LOCAL bool configValueKeyParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  Key        *key = (Key*)variable;
  Errors     error;
  uint       dataLength;
  void       *data;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (File_existsCString(value))
  {
    // read key data from file

    error = Configuration_readKeyFile(key,value);
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,errorMessageSize,Error_getText(error));
      return FALSE;
    }
  }
  else if (stringStartsWith(value,"base64:"))
  {
    // decode base64 encoded key data

    // get key data length
    dataLength = Misc_base64DecodeLengthCString(&value[7]);
    if (dataLength > 0)
    {
      // allocate key memory
      data = allocSecure(dataLength);
      if (data == NULL)
      {
        stringSet(errorMessage,errorMessageSize,"insufficient secure memory");
        return FALSE;
      }

      // decode base64
      if (!Misc_base64DecodeCString((byte*)data,dataLength,NULL,&value[7]))
      {
        stringSet(errorMessage,errorMessageSize,"decode base64 fail");
        freeSecure(data);
        return FALSE;
      }

      // set key data
      if (key->data != NULL) freeSecure(key->data);
      key->data   = data;
      key->length = dataLength;
    }
  }
  else
  {
    // get plain key data

    // get key data length
    dataLength = stringLength(value);
    if (dataLength > 0)
    {
      // allocate key memory
      data = allocSecure(dataLength);
      if (data == NULL)
      {
        stringSet(errorMessage,errorMessageSize,"insufficient secure memory");
        return FALSE;
      }

      // copy data
      memCopyFast(data,dataLength,value,dataLength);

      // set key data
      if (key->data != NULL) freeSecure(key->data);
      key->data   = data;
      key->length = dataLength;
    }
  }

  return TRUE;
}

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

LOCAL bool configValueKeyFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (Key*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<file name>|base64:<data>|<data>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const Key *key = (Key*)(*formatUserData);
        String    line = (String)data;

        if (key != NULL)
        {
          if (Configuration_isKeyAvailable(key))
          {
            String_appendCString(line,"base64:");
            Misc_base64Encode(line,key->data,key->length);
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

LOCAL bool configValuePublicPrivateKeyParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  CryptKey *cryptKey = (CryptKey*)variable;
  Errors   error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (File_existsCString(value))
  {
    // read key data from file

    // read key file
    error = Crypt_readPublicPrivateKeyFile(cryptKey,
                                           value,
                                           CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                           CRYPT_KEY_DERIVE_NONE,
                                           NULL,  // cryptSalt
                                           NULL  // password
                                          );

    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,errorMessageSize,Error_getText(error));
      return FALSE;
    }
  }
  else if (stringStartsWith(value,"base64:"))
  {
    // base64-prefixed key string

    // set crypt key data
    error = Crypt_setPublicPrivateKeyData(cryptKey,
                                          &value[7],
                                          stringLength(value)-7,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password
                                         );
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,errorMessageSize,Error_getText(error));
      return FALSE;
    }
  }
  else
  {
    // get plain key data

    // set crypt key data
    error = Crypt_setPublicPrivateKeyData(cryptKey,
                                          value,
                                          stringLength(value),
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password
                                         );
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,errorMessageSize,Error_getText(error));
      return FALSE;
    }
  }

  return TRUE;
}

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

LOCAL bool configValueHashDataParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  Hash                *hash = (Hash*)variable;
  char                cryptHashAlgorithmName[64];
  char                salt[32];
  CryptHash           cryptHash;
  CryptHashAlgorithms cryptHashAlgorithm;
  long                offset;
  String              string;
  uint                dataLength;
  void                *data;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

//fprintf(stderr,"%s, %d: name=%s value=%s\n",__FILE__,__LINE__,name,value);
  if      (String_parseCString(value,"%64s:%32s:",&offset,cryptHashAlgorithmName,salt))
  {
    // <hash algorithm>:<salt>:<hash> -> get hash

    // get hash algorithm
    if (!Crypt_parseHashAlgorithm(cryptHashAlgorithmName,&cryptHashAlgorithm))
    {
      return FALSE;
    }

    dataLength = Misc_base64DecodeLengthCString(&value[offset]);
    if (dataLength > 0)
    {
      // allocate secure memory
      data = allocSecure((size_t)dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // decode base64
//TODO: use salt?
      if (!Misc_base64DecodeCString((byte*)data,dataLength,NULL,&value[offset]))
      {
        freeSecure(data);
        return FALSE;
      }
    }
    else
    {
      data = NULL;
    }
  }
  else if (String_parseCString(value,"%64s:",&offset,cryptHashAlgorithmName))
  {
    // <hash algorithm>:<hash> -> get hash

    // get hash algorithm
    if (!Crypt_parseHashAlgorithm(cryptHashAlgorithmName,&cryptHashAlgorithm))
    {
      return FALSE;
    }

    dataLength = Misc_base64DecodeLengthCString(&value[offset]);
    if (dataLength > 0)
    {
      // allocate secure memory
      data = allocSecure((size_t)dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // decode base64
      if (!Misc_base64DecodeCString((byte*)data,dataLength,NULL,&value[offset]))
      {
        freeSecure(data);
        return FALSE;
      }
    }
    else
    {
      data = NULL;
    }
  }
  else if (!stringIsEmpty(value))
  {
    // <plain data> -> calculate hash

    // unquote/unescape
    string = String_newCString(value);
    String_unquote(string,STRING_QUOTES);
    String_unescape(string,
                    STRING_ESCAPE_CHARACTER,
                    STRING_ESCAPE_CHARACTERS_MAP_TO,
                    STRING_ESCAPE_CHARACTERS_MAP_FROM,
                    STRING_ESCAPE_CHARACTER_MAP_LENGTH
                  );

    // use default hash alogorithm
    cryptHashAlgorithm = PASSWORD_HASH_ALGORITHM;

    // calculate hash
    Crypt_initHash(&cryptHash,cryptHashAlgorithm);
    Crypt_updateHash(&cryptHash,String_cString(string),String_length(string));
//fprintf(stderr,"%s, %d: value='%s'\n",__FILE__,__LINE__,value); Crypt_dumpHash(&cryptHash);

    dataLength = Crypt_getHashLength(&cryptHash);
    if (dataLength > 0)
    {
      // allocate secure memory
      data = allocSecure((size_t)dataLength);
      if (data == NULL)
      {
        Crypt_doneHash(&cryptHash);
        String_delete(string);
        return FALSE;
      }

      // get hash data
      Crypt_getHash(&cryptHash,data,dataLength,NULL);
    }
    else
    {
      data = NULL;
    }

    // free resources
    Crypt_doneHash(&cryptHash);
    String_delete(string);

    // mark config modified
    configModified = TRUE;
  }
  else
  {
    // none
    cryptHashAlgorithm = CRYPT_HASH_ALGORITHM_NONE;
    data               = NULL;
    dataLength         = 0;
  }

  // set hash data
  if (hash->data != NULL) freeSecure(hash->data);
  hash->cryptHashAlgorithm = cryptHashAlgorithm;
  hash->data               = data;
  hash->length             = dataLength;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueHashDataFormat
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

LOCAL bool configValueHashDataFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (Hash*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<file name>|base64:<data>|<data>");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const Hash *hash = (const Hash*)(*formatUserData);
        String     line  = (String)data;

        if (hash != NULL)
        {
          if (Configuration_isHashAvailable(hash))
          {
            String_appendFormat(line,"%s:",Crypt_hashAlgorithmToString(hash->cryptHashAlgorithm,NULL));
            Misc_base64Encode(line,hash->data,hash->length);
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : readConfigFile
* Purpose: read configuration from file
* Input  : fileName      - file name
*          printInfoFlag - TRUE for output info, FALSE otherwise
* Output : -
* Return : TRUE iff configuration read, FALSE otherwise (error)
* Notes  : -
\***********************************************************************/

LOCAL bool readConfigFile(ConstString fileName, bool printInfoFlag)
{
  FileInfo   fileInfo;
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     name,value;
  StringList commentLineList;
  long       nextIndex;

  assert(fileName != NULL);

  // check file permissions
  error = File_getInfo(&fileInfo,fileName);
  if (error == ERROR_NONE)
  {
    if ((fileInfo.permission & (FILE_PERMISSION_GROUP_READ|FILE_PERMISSION_OTHER_READ)) != 0)
    {
      printWarning(_("Configuration file '%s' has wrong file permission %03o. Please make sure read permissions are limited to file owner (mode 600)"),
                   String_cString(fileName),
                   fileInfo.permission & FILE_PERMISSION_MASK
                  );
    }
  }
  else
  {
    printWarning(_("Cannot get file info for configuration file '%s' (error: %s)"),
                 String_cString(fileName),
                 Error_getText(error)
                );
  }

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printWarning(_("Cannot open configuration file '%s' (error: %s)!"),
                 String_cString(fileName),
                 Error_getText(error)
                );
    return TRUE;
  }

  // parse file
  failFlag   = FALSE;
  line       = String_new();
  lineNb     = 0;
  name       = String_new();
  value      = String_new();
  StringList_init(&commentLineList);
//TODO: remove
extern Semaphore       consoleLock;            // lock console
  SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (printInfoFlag) { printConsole(stdout,0,"Reading configuration file '%s'...",String_cString(fileName)); }
    while (   !failFlag
           && File_getLine(&fileHandle,line,&lineNb,NULL)
          )
    {
      // parse line
//fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(line));
      String_trim(line,STRING_WHITE_SPACES);
      if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
      {
        // discard comments if separator or empty line
        StringList_clear(&commentLineList);
      }
      else if (String_startsWithCString(line,"# "))
      {
        // store comment
        String_remove(line,STRING_BEGIN,2);
        StringList_append(&commentLineList,line);
      }
      else if (String_startsWithChar(line,'#'))
      {
        // ignore commented values
      }
      else if (String_parse(line,STRING_BEGIN,"[file-server %S]",NULL,name))
      {
        ServerNode *serverNode;

        // find/allocate server node
        serverNode = NULL;
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,
                                              serverNode,
                                                 (serverNode->type == SERVER_TYPE_FILE)
                                              && String_equals(serverNode->name,name)
                                             );
          if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
        }
        if (serverNode == NULL) serverNode = Configuration_newServerNode(name,SERVER_TYPE_FILE);
        assert(serverNode != NULL);
        assert(serverNode->type == SERVER_TYPE_FILE);

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,NULL)
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
          {
            // discard comments if separator or empty line
            StringList_clear(&commentLineList);
          }
          else if (String_startsWithCString(line,"# "))
          {
            // store comment
            String_remove(line,STRING_BEGIN,2);
            StringList_append(&commentLineList,line);
          }
          else if (String_startsWithChar(line,'#'))
          {
            // ignore commented values
          }
          else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            (void)ConfigValue_parse(String_cString(name),
                                    String_cString(value),
                                    CONFIG_VALUES,
                                    "file-server",
                                    CALLBACK_LAMBDA_(void,(const char *errorMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      printError("%s in section '%s' in %s, line %ld",errorMessage,"file-server",String_cString(fileName),lineNb);
                                      failFlag = TRUE;
                                    },NULL),
                                    CALLBACK_LAMBDA_(void,(const char *warningMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      printWarning("%s in section '%s' in %s, line %ld",warningMessage,"file-server",String_cString(fileName),lineNb);
                                    },NULL),
                                    serverNode,
                                    &commentLineList
                                   );
            assert(StringList_isEmpty(&commentLineList));
            if (failFlag) break;
          }
          else
          {
            if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
            printError(_("Syntax error in '%s', line %ld: '%s'"),
                       String_cString(fileName),
                       lineNb,
                       String_cString(line)
                      );
            failFlag = TRUE;
            break;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // add to server list
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          List_append(&globalOptions.serverList,serverNode);
        }
      }
      else if (String_parse(line,STRING_BEGIN,"[ftp-server %S]",NULL,name))
      {
        ServerNode *serverNode;

        // find/allocate server node
        serverNode = NULL;
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,
                                              serverNode,
                                                 (serverNode->type == SERVER_TYPE_FTP)
//TODO: port number
                                              && String_equals(serverNode->name,name)
                                             );
          if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
        }
        if (serverNode == NULL) serverNode = Configuration_newServerNode(name,SERVER_TYPE_FTP);
        assert(serverNode != NULL);
        assert(serverNode->type == SERVER_TYPE_FTP);

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,NULL)
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
          {
            // discard comments if separator or empty line
            StringList_clear(&commentLineList);
          }
          else if (String_startsWithCString(line,"# "))
          {
            // store comment
            String_remove(line,STRING_BEGIN,2);
            StringList_append(&commentLineList,line);
          }
          else if (String_startsWithChar(line,'#'))
          {
            // ignore commented values
          }
          else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            (void)ConfigValue_parse(String_cString(name),
                                    String_cString(value),
                                    CONFIG_VALUES,
                                    "ftp-server",
                                    CALLBACK_LAMBDA_(void,(const char *errorMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printError("%s in section '%s' in %s, line %ld",errorMessage,"ftp-server",String_cString(fileName),lineNb);
                                      failFlag = TRUE;
                                    },NULL),
                                    CALLBACK_LAMBDA_(void,(const char *warningMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printWarning("%s in section '%s' in %s, line %ld",warningMessage,"ftp-server",String_cString(fileName),lineNb);
                                    },NULL),
                                    serverNode,
                                    &commentLineList
                                   );
            assert(StringList_isEmpty(&commentLineList));
            if (failFlag) break;
          }
          else
          {
            if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
            printError(_("Syntax error in '%s', line %ld: '%s'"),
                       String_cString(fileName),
                       lineNb,
                       String_cString(line)
                      );
            failFlag = TRUE;
            break;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // add to server list
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          List_append(&globalOptions.serverList,serverNode);
        }
      }
      else if (String_parse(line,STRING_BEGIN,"[ssh-server %S]",NULL,name))
      {
        ServerNode *serverNode;

        // find/allocate server node
        serverNode = NULL;
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,
                                              serverNode,
                                                 (serverNode->type == SERVER_TYPE_SSH)
//TODO: port number
                                              && String_equals(serverNode->name,name)
                                             );
          if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
        }
        if (serverNode == NULL) serverNode = Configuration_newServerNode(name,SERVER_TYPE_SSH);
        assert(serverNode != NULL);
        assert(serverNode->type == SERVER_TYPE_SSH);

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,NULL)
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
          {
            // discard comments if separator or empty line
            StringList_clear(&commentLineList);
          }
          else if (String_startsWithCString(line,"# "))
          {
            // store comment
            String_remove(line,STRING_BEGIN,2);
            StringList_append(&commentLineList,line);
          }
          else if (String_startsWithChar(line,'#'))
          {
            // ignore commented values
          }
          else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            (void)ConfigValue_parse(String_cString(name),
                                    String_cString(value),
                                    CONFIG_VALUES,
                                    "ssh-server",
                                    CALLBACK_LAMBDA_(void,(const char *errorMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printError("%s in section '%s' in %s, line %ld",errorMessage,"ssh-server",String_cString(fileName),lineNb);
                                      failFlag = TRUE;
                                    },NULL),
                                    CALLBACK_LAMBDA_(void,(const char *warningMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printWarning("%s in section '%s' in %s, line %ld",warningMessage,"ssh-server",String_cString(fileName),lineNb);
                                    },NULL),
                                    serverNode,
                                    &commentLineList
                                   );
            assert(StringList_isEmpty(&commentLineList));
            if (failFlag) break;
          }
          else
          {
            if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
            printError(_("Syntax error in '%s', line %ld: '%s'"),
                       String_cString(fileName),
                       lineNb,
                       String_cString(line)
                      );
            failFlag = TRUE;
            break;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // add to server list
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          List_append(&globalOptions.serverList,serverNode);
        }
      }
      else if (String_parse(line,STRING_BEGIN,"[webdav-server %S]",NULL,name))
      {
        ServerNode *serverNode;

        // find/allocate server node
        serverNode = NULL;
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,
                                              serverNode,
                                                 (serverNode->type == SERVER_TYPE_WEBDAV)
//TODO: port number
                                              && String_equals(serverNode->name,name)
                                             );
          if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
        }
        if (serverNode == NULL) serverNode = Configuration_newServerNode(name,SERVER_TYPE_WEBDAV);
        assert(serverNode != NULL);
        assert(serverNode->type == SERVER_TYPE_WEBDAV);

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,NULL)
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
          {
            // discard comments if separator or empty line
            StringList_clear(&commentLineList);
          }
          else if (String_startsWithCString(line,"# "))
          {
            // store comment
            String_remove(line,STRING_BEGIN,2);
            StringList_append(&commentLineList,line);
          }
          else if (String_startsWithChar(line,'#'))
          {
            // ignore commented values
          }
          else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            (void)ConfigValue_parse(String_cString(name),
                                    String_cString(value),
                                    CONFIG_VALUES,
                                    "webdav-server",
                                    CALLBACK_LAMBDA_(void,(const char *errorMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printError("%s in section '%s' in %s, line %ld",errorMessage,"webdav-server",String_cString(fileName),lineNb);
                                      failFlag = TRUE;
                                    },NULL),
                                    CALLBACK_LAMBDA_(void,(const char *warningMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printWarning("%s in section '%s' in %s, line %ld",warningMessage,"webdav-server",String_cString(fileName),lineNb);
                                    },NULL),
                                    serverNode,
                                    &commentLineList
                                   );
            assert(StringList_isEmpty(&commentLineList));
            if (failFlag) break;
          }
          else
          {
            if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
            printError(_("Syntax error in '%s', line %ld: '%s'"),
                       String_cString(fileName),
                       lineNb,
                       String_cString(line)
                      );
            failFlag = TRUE;
            break;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // add to server list
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          List_append(&globalOptions.serverList,serverNode);
        }
      }
      else if (String_parse(line,STRING_BEGIN,"[device %S]",NULL,name))
      {
        DeviceNode *deviceNode;

        // find/allocate device node
        deviceNode = NULL;
        SEMAPHORE_LOCKED_DO(&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          deviceNode = (DeviceNode*)LIST_FIND(&globalOptions.deviceList,deviceNode,String_equals(deviceNode->device.name,name));
          if (deviceNode != NULL) List_remove(&globalOptions.deviceList,deviceNode);
        }
        if (deviceNode == NULL) deviceNode = newDeviceNode(name);

        // parse section
        while (   File_getLine(&fileHandle,line,&lineNb,NULL)
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
          {
            // discard comments if separator or empty line
            StringList_clear(&commentLineList);
          }
          else if (String_startsWithCString(line,"# "))
          {
            // store comment
            String_remove(line,STRING_BEGIN,2);
            StringList_append(&commentLineList,line);
          }
          else if (String_startsWithChar(line,'#'))
          {
            // ignore commented values
          }
          else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            (void)ConfigValue_parse(String_cString(name),
                                    String_cString(value),
                                    CONFIG_VALUES,
                                    "device",
                                    LAMBDA(void,(const char *errorMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printError("%s in section '%s' in %s, line %ld",errorMessage,"device-server",String_cString(fileName),lineNb);
                                      failFlag = TRUE;
                                    }),NULL,
                                    LAMBDA(void,(const char *warningMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printWarning("%s in section '%s' in %s, line %ld",warningMessage,"device-server",String_cString(fileName),lineNb);
                                    }),NULL,
                                    &deviceNode->device,
                                    &commentLineList
                                   );
            assert(StringList_isEmpty(&commentLineList));
            if (failFlag) break;
          }
          else
          {
            if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
            printError(_("Syntax error in '%s', line %ld: '%s'"),
                       String_cString(fileName),
                       lineNb,
                       String_cString(line)
                      );
            failFlag = TRUE;
            break;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // add to device list
        SEMAPHORE_LOCKED_DO(&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          List_append(&globalOptions.deviceList,deviceNode);
        }
      }
      else if (String_parse(line,STRING_BEGIN,"[master]",NULL))
      {
StringList_clear(&commentLineList);
        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,NULL)
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
          {
            // discard comments if separator or empty line
            StringList_clear(&commentLineList);
          }
          else if (String_startsWithCString(line,"# "))
          {
            // store comment
            String_remove(line,STRING_BEGIN,2);
            StringList_append(&commentLineList,line);
          }
          else if (String_startsWithChar(line,'#'))
          {
            // ignore commented values
          }
          else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            (void)ConfigValue_parse(String_cString(name),
                                    String_cString(value),
                                    CONFIG_VALUES,
                                    "master",
                                    LAMBDA(void,(const char *errorMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printError("%s in section '%s' in %s, line %ld",errorMessage,"master",String_cString(fileName),lineNb);
                                      failFlag = TRUE;
                                    }),NULL,
                                    LAMBDA(void,(const char *warningMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                      printWarning("%s in section '%s' in %s, line %ld",warningMessage,"master",String_cString(fileName),lineNb);
                                    }),NULL,
                                    &globalOptions.masterInfo,
                                    &commentLineList
                                   );
            assert(StringList_isEmpty(&commentLineList));
            if (failFlag) break;
          }
          else
          {
            if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
            printError(_("Syntax error in '%s', line %ld: '%s'"),
                       String_cString(fileName),
                       lineNb,
                       String_cString(line)
                      );
            failFlag = TRUE;
            break;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);
      }
      else if (String_parse(line,STRING_BEGIN,"[maintenance]",NULL))
      {
        MaintenanceNode *maintenanceNode;

        // allocate new maintenance time node
        maintenanceNode = Configuration_newMaintenanceNode();

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,NULL)
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
          {
            // discard comments if separator or empty line
            StringList_clear(&commentLineList);
          }
          else if (String_startsWithCString(line,"# "))
          {
            // store comment
            String_remove(line,STRING_BEGIN,2);
            StringList_append(&commentLineList,line);
          }
          else if (String_startsWithChar(line,'#'))
          {
            // ignore commented values
          }
          else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            (void)ConfigValue_parse(String_cString(name),
                                    String_cString(value),
                                    CONFIG_VALUES,
                                    "maintenance",
                                    CALLBACK_LAMBDA_(void,(const char *errorMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      printError("%s in section '%s' in %s, line %ld",errorMessage,"maintenance",String_cString(fileName),lineNb);
                                      failFlag = TRUE;
                                    },NULL),
                                    CALLBACK_LAMBDA_(void,(const char *warningMessage, void *userData),
                                    {
                                      UNUSED_VARIABLE(userData);

                                      printWarning("%s in section '%s' in %s, line %ld",warningMessage,"maintenance",String_cString(fileName),lineNb);
                                    },NULL),
                                    maintenanceNode,
                                    &commentLineList
                                   );
            assert(StringList_isEmpty(&commentLineList));
            if (failFlag) break;
          }
          else
          {
            if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
            printError(_("Syntax error in '%s', line %ld: '%s'"),
                       String_cString(fileName),
                       lineNb,
                       String_cString(line)
                      );
            failFlag = TRUE;
            break;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // add to maintenance list
        SEMAPHORE_LOCKED_DO(&globalOptions.maintenanceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          List_append(&globalOptions.maintenanceList,maintenanceNode);
        }
      }
      else if (String_parse(line,STRING_BEGIN,"[global]",NULL))
      {
        // nothing to do
      }
      else if (String_parse(line,STRING_BEGIN,"[end]",NULL))
      {
        // nothing to do
      }
      else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
      {
        (void)ConfigValue_parse(String_cString(name),
                                String_cString(value),
                                CONFIG_VALUES,
                                NULL, // section name
                                LAMBDA(void,(const char *errorMessage, void *userData),
                                {
                                  UNUSED_VARIABLE(userData);

                                  if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                  printError("%s in %s, line %ld",errorMessage,String_cString(fileName),lineNb);
                                  failFlag = TRUE;
                                }),NULL,
                                LAMBDA(void,(const char *warningMessage, void *userData),
                                {
                                  UNUSED_VARIABLE(userData);

                                  if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
                                  printWarning("%s in %s, line %ld",warningMessage,String_cString(fileName),lineNb);
                                }),NULL,
                                NULL,  // variable
                                &commentLineList
                               );
        assert(StringList_isEmpty(&commentLineList));
        if (failFlag) break;
      }
      else
      {
        if (printInfoFlag) printConsole(stdout,0,"FAIL!\n");
        printError(_("Unknown config entry '%s' in %s, line %ld"),
                   String_cString(line),
                   String_cString(fileName),
                   lineNb
                  );
        failFlag = TRUE;
        break;
      }
    }
    if (!failFlag)
    {
      if (printInfoFlag) { printConsole(stdout,0,"OK\n"); }
    }
  }
  StringList_done(&commentLineList);

  // free resources
  String_delete(value);
  String_delete(name);
  String_delete(line);

  // close file
  (void)File_close(&fileHandle);

  return !failFlag;
}

/*---------------------------------------------------------------------*/

CommandLineOption COMMAND_LINE_OPTIONS[] = CMD_VALUE_ARRAY
(
  CMD_OPTION_ENUM         ("create",                            'c',0,1,globalOptions.command,                            0,COMMAND_CREATE_FILES,                                        "create new files archives"                                                ),
  CMD_OPTION_ENUM         ("image",                             'm',0,1,globalOptions.command,                            0,COMMAND_CREATE_IMAGES,                                       "create new images archives"                                               ),
  CMD_OPTION_ENUM         ("list",                              'l',0,1,globalOptions.command,                            0,COMMAND_LIST,                                                "list contents of archives"                                                ),
  CMD_OPTION_ENUM         ("test",                              't',0,1,globalOptions.command,                            0,COMMAND_TEST,                                                "test contents of archives"                                                ),
  CMD_OPTION_ENUM         ("compare",                           'd',0,1,globalOptions.command,                            0,COMMAND_COMPARE,                                             "compare contents of archive swith files and images"                       ),
  CMD_OPTION_ENUM         ("extract",                           'x',0,1,globalOptions.command,                            0,COMMAND_RESTORE,                                             "restore archives"                                                         ),
  CMD_OPTION_ENUM         ("convert",                           0,  0,1,globalOptions.command,                            0,COMMAND_CONVERT,                                             "convert archives"                                                         ),
  CMD_OPTION_ENUM         ("generate-keys",                     0,  0,1,globalOptions.command,                            0,COMMAND_GENERATE_ENCRYPTION_KEYS,                            "generate new public/private key pair for encryption"                      ),
  CMD_OPTION_ENUM         ("generate-signature-keys",           0,  0,1,globalOptions.command,                            0,COMMAND_GENERATE_SIGNATURE_KEYS,                             "generate new public/private key pair for signature"                       ),
//  CMD_OPTION_ENUM         ("new-key-password",                  0,  1,1,command,                                          0,COMMAND_NEW_KEY_PASSWORD,                                    "set new private key password"                                             ),
  CMD_OPTION_INTEGER      ("generate-keys-bits",                0,  1,1,globalOptions.generateKeyBits,                    0,MIN_ASYMMETRIC_CRYPT_KEY_BITS,
                                                                                                                            MAX_ASYMMETRIC_CRYPT_KEY_BITS,COMMAND_LINE_BITS_UNITS,       "key bits",NULL                                                            ),
  CMD_OPTION_SELECT       ("generate-keys-mode",                0,  1,2,globalOptions.generateKeyMode,                    0,COMMAND_LINE_OPTIONS_GENERATE_KEY_MODES,                     "select generate key mode mode"                                            ),
  CMD_OPTION_STRING       ("job",                               0,  0,1,globalOptions.jobUUIDOrName,                      0,                                                             "execute job","name or UUID"                                               ),

  CMD_OPTION_ENUM         ("normal",                            0,  1,2,globalOptions.archiveType,                        0,ARCHIVE_TYPE_NORMAL,                                         "create normal archive (no incremental list file, default)"                ),
  CMD_OPTION_ENUM         ("full",                              'f',0,2,globalOptions.archiveType,                        0,ARCHIVE_TYPE_FULL,                                           "create full archive and incremental list file"                            ),
  CMD_OPTION_ENUM         ("incremental",                       'i',0,2,globalOptions.archiveType,                        0,ARCHIVE_TYPE_INCREMENTAL,                                    "create incremental archive"                                               ),
  CMD_OPTION_SPECIAL      ("incremental-list-file",             'I',1,2,&globalOptions.incrementalListFileName,           0,cmdOptionParseString,NULL,1,                                 "incremental list file name (default: <archive name>.bid)","file name"     ),
  CMD_OPTION_ENUM         ("differential",                      0,  1,2,globalOptions.archiveType,                        0,ARCHIVE_TYPE_DIFFERENTIAL,                                   "create differential archive"                                              ),

  CMD_OPTION_SELECT       ("pattern-type",                      0,  1,2,globalOptions.patternType,                        0,COMMAND_LINE_OPTIONS_PATTERN_TYPES,                          "select pattern type"                                                      ),

  CMD_OPTION_BOOLEAN      ("storage-list-stdin",                'T',1,3,globalOptions.storageNameListStdin,               0,                                                             "read storage name list from stdin"                                        ),
  CMD_OPTION_STRING       ("storage-list",                      0,  1,3,globalOptions.storageNameListFileName,            0,                                                             "storage name list file name","file name"                                  ),
  CMD_OPTION_STRING       ("storage-command",                   0,  1,3,globalOptions.storageNameCommand,                 0,                                                             "storage name command","command"                                           ),

  CMD_OPTION_SPECIAL      ("include",                           '#',0,3,&globalOptions.includeEntryList,                  0,cmdOptionParseEntryPattern,NULL,1,                           "include pattern","pattern"                                                ),
  CMD_OPTION_STRING       ("include-file-list",                 0,  1,3,globalOptions.includeFileListFileName,            0,                                                             "include file pattern list file name","file name"                          ),
  CMD_OPTION_STRING       ("include-file-command",              0,  1,3,globalOptions.includeFileCommand,                 0,                                                             "include file pattern command","command"                                   ),
  CMD_OPTION_STRING       ("include-image-list",                0,  1,3,globalOptions.includeImageListFileName,           0,                                                             "include image pattern list file name","file name"                         ),
  CMD_OPTION_STRING       ("include-image-command",             0,  1,3,globalOptions.includeImageCommand,                0,                                                             "include image pattern command","command"                                  ),
  CMD_OPTION_SPECIAL      ("exclude",                           '!',0,3,&globalOptions.excludePatternList,                0,cmdOptionParsePattern,NULL,1,                                "exclude pattern","pattern"                                                ),
  CMD_OPTION_STRING       ("exclude-list",                      0,  1,3,globalOptions.excludeListFileName,                0,                                                             "exclude pattern list file name","file name"                               ),
  CMD_OPTION_STRING       ("exclude-command",                   0,  1,3,globalOptions.excludeCommand,                     0,                                                             "exclude pattern command","command"                                        ),

  CMD_OPTION_SPECIAL      ("mount",                             0  ,1,3,&globalOptions.mountList,                         0,cmdOptionParseMount,NULL,1,                                  "mount device","name[,[device][,yes]"                                      ),
  CMD_OPTION_STRING       ("mount-command",                     0,  1,3,globalOptions.mountCommand,                       0,                                                             "mount command","command"                                                  ),
  CMD_OPTION_STRING       ("mount-device-command",              0,  1,3,globalOptions.mountDeviceCommand,                 0,                                                             "mount device command","command"                                           ),
  CMD_OPTION_STRING       ("unmount-command",                   0,  1,3,globalOptions.unmountCommand,                     0,                                                             "unmount command","command"                                                ),

  CMD_OPTION_SPECIAL      ("delta-source",                      0,  0,3,&globalOptions.deltaSourceList,                   0,cmdOptionParseDeltaSource,NULL,1,                            "source pattern","pattern"                                                 ),

  CMD_OPTION_SPECIAL      ("config",                            0,  1,2,&configFileList,                                  0,cmdOptionParseConfigFile,NULL,1,                             "configuration file","file name"                                           ),

  CMD_OPTION_STRING       ("tmp-directory",                     0,  1,1,globalOptions.tmpDirectory,                       0,                                                             "temporary directory","path"                                               ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",                      0,  1,1,globalOptions.maxTmpSize,                         0,0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "max. size of temporary files","unlimited"                                 ),

  CMD_OPTION_INTEGER64    ("archive-part-size",                 's',0,2,globalOptions.archivePartSize,                    0,0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "approximated archive part size","unlimited"                               ),
  CMD_OPTION_INTEGER64    ("fragment-size",                     0,  0,3,globalOptions.fragmentSize,                       0,0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "fragment size","unlimited"                                                ),

  CMD_OPTION_INTEGER      ("directory-strip",                   'p',1,2,globalOptions.directoryStripCount,                0,-1,MAX_INT,NULL,                                             "number of directories to strip on extract",NULL                           ),
  CMD_OPTION_STRING       ("destination",                       0,  0,2,globalOptions.destination,                        0,                                                             "destination to restore entries","path"                                    ),
  CMD_OPTION_SPECIAL      ("owner",                             0,  0,2,&globalOptions.owner,                             0,cmdOptionParseOwner,NULL,1,                                  "user and group of restored files","user:group"                            ),
  CMD_OPTION_SPECIAL      ("permissions",                       0,  0,2,&globalOptions.permissions,                       0,cmdOptionParsePermissions,NULL,1,                            "permissions of restored files","<owner>:<group>:<world>|<number>"         ),

  CMD_OPTION_STRING       ("comment",                           0,  1,1,globalOptions.comment,                            GLOBAL_OPTION_SET_COMMENT,                                     "comment","text"                                                           ),

  CMD_OPTION_CSTRING      ("directory",                         'C',1,0,globalOptions.changeToDirectory,                                0,                                                             "change to directory","path"                                               ),

  CMD_OPTION_SPECIAL      ("compress-algorithm",                'z',0,2,&globalOptions.compressAlgorithms,                GLOBAL_OPTION_SET_COMPRESS_ALGORITHMS,cmdOptionParseCompressAlgorithms,NULL,1,"select compress algorithms to use\n"
                                                                                                                                                                                            "  none         : no compression (default)\n"
                                                                                                                                                                                            "  zip0..zip9   : ZIP compression level 0..9"
                                                                                                                                                                                            #ifdef HAVE_BZ2
                                                                                                                                                                                            "\n"
                                                                                                                                                                                            "  bzip1..bzip9 : BZIP2 compression level 1..9"
                                                                                                                                                                                            #endif
                                                                                                                                                                                            #ifdef HAVE_LZMA
                                                                                                                                                                                            "\n"
                                                                                                                                                                                            "  lzma1..lzma9 : LZMA compression level 1..9"
                                                                                                                                                                                            #endif
                                                                                                                                                                                            #ifdef HAVE_LZO
                                                                                                                                                                                            "\n"
                                                                                                                                                                                            "  lzo1..lzo5   : LZO compression level 1..5"
                                                                                                                                                                                            #endif
                                                                                                                                                                                            #ifdef HAVE_LZ4
                                                                                                                                                                                            "\n"
                                                                                                                                                                                            "  lz4-0..lz4-16: LZ4 compression level 0..16"
                                                                                                                                                                                            #endif
                                                                                                                                                                                            #ifdef HAVE_ZSTD
                                                                                                                                                                                            "\n"
                                                                                                                                                                                            "  zstd0..zstd19: ZStd compression level 0..19"
                                                                                                                                                                                            #endif
                                                                                                                                                                                            #ifdef HAVE_XDELTA
                                                                                                                                                                                            "\n"
                                                                                                                                                                                            "additional select with '+':\n"
                                                                                                                                                                                            "  xdelta1..xdelta9: XDELTA compression level 1..9"
                                                                                                                                                                                            #endif
                                                                                                                                                                                            ,
                                                                                                                                                                                            "algorithm|xdelta+algorithm"                                               ),
  CMD_OPTION_INTEGER      ("compress-min-size",                 0,  1,2,globalOptions.compressMinFileSize,                0,0,MAX_INT,COMMAND_LINE_BYTES_UNITS,                          "minimal size of file for compression",NULL                                ),
  CMD_OPTION_SPECIAL      ("compress-exclude",                  0,  0,3,&globalOptions.compressExcludePatternList,        0,cmdOptionParsePattern,NULL,1,                                "exclude compression pattern","pattern"                                    ),

  CMD_OPTION_SPECIAL      ("crypt-algorithm",                   'y',0,2,globalOptions.cryptAlgorithms,                    GLOBAL_OPTION_SET_CRYPT_ALGORITHMS,cmdOptionParseCryptAlgorithms,NULL,1,"select crypt algorithms to use\n"
                                                                                                                                                                                            "  none (default)"
                                                                                                                                                                                            #ifdef HAVE_GCRYPT
                                                                                                                                                                                            "\n"
                                                                                                                                                                                            "  3DES\n"
                                                                                                                                                                                            "  CAST5\n"
                                                                                                                                                                                            "  BLOWFISH\n"
                                                                                                                                                                                            "  AES128\n"
                                                                                                                                                                                            "  AES192\n"
                                                                                                                                                                                            "  AES256\n"
                                                                                                                                                                                            "  TWOFISH128\n"
                                                                                                                                                                                            "  TWOFISH256\n"
                                                                                                                                                                                            "  SERPENT128\n"
                                                                                                                                                                                            "  SERPENT192\n"
                                                                                                                                                                                            "  SERPENT256\n"
                                                                                                                                                                                            "  CAMELLIA128\n"
                                                                                                                                                                                            "  CAMELLIA192\n"
                                                                                                                                                                                            "  CAMELLIA256"
                                                                                                                                                                                            #endif
                                                                                                                                                                                            ,
                                                                                                                                                                                            "algorithm"                                                                ),
  CMD_OPTION_SELECT       ("crypt-type",                        0,  0,2,globalOptions.cryptType,                          0,COMMAND_LINE_OPTIONS_CRYPT_TYPES,                            "select crypt type"                                                        ),
  CMD_OPTION_SPECIAL      ("crypt-password",                    0,  0,2,&globalOptions.cryptPassword,                     0,cmdOptionParsePassword,NULL,1,                               "crypt password (use with care!)","password"                               ),
  CMD_OPTION_SPECIAL      ("crypt-new-password",                0,  0,2,&globalOptions.cryptNewPassword,                  0,cmdOptionParsePassword,NULL,1,                               "new crypt password (use with care!)","password"                           ),
//#warning remove/revert
  CMD_OPTION_SPECIAL      ("crypt-public-key",                  0,  0,2,&globalOptions.cryptPublicKey,                    0,cmdOptionParseKey,NULL,1,                                    "public key for asymmetric encryption","file name|data"                    ),
  CMD_OPTION_SPECIAL      ("crypt-private-key",                 0,  0,2,&globalOptions.cryptPrivateKey,                   0,cmdOptionParseKey,NULL,1,                                    "private key for asymmetric decryption","file name|data"                   ),
  CMD_OPTION_SPECIAL      ("signature-public-key",              0,  0,1,&globalOptions.signaturePublicKey,                0,cmdOptionParseKey,NULL,1,                                    "public key for signature check","file name|data"                          ),
  CMD_OPTION_SPECIAL      ("signature-private-key",             0,  0,2,&globalOptions.signaturePrivateKey,               0,cmdOptionParseKey,NULL,1,                                    "private key for signature generation","file name|data"                    ),

//TODO
//  CMD_OPTION_INTEGER64    ("file-max-storage-size",              0,  0,2,defaultFileServer.maxStorageSize,                0,0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on file server","unlimited"                  ),

  CMD_OPTION_STRING       ("ftp-login-name",                    0,  0,2,globalOptions.defaultFTPServer.ftp.loginName,                   0,                                                             "ftp login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ftp-password",                      0,  0,2,&globalOptions.defaultFTPServer.ftp.password,                   0,cmdOptionParsePassword,NULL,1,                               "ftp password (use with care!)","password"                                 ),
  CMD_OPTION_INTEGER      ("ftp-max-connections",               0,  0,2,globalOptions.defaultFTPServer.maxConnectionCount,              0,0,MAX_INT,NULL,                                              "max. number of concurrent ftp connections","unlimited"                    ),
//TODO
//  CMD_OPTION_INTEGER64    ("ftp-max-storage-size",              0,  0,2,defaultFTPServer.maxStorageSize,                  NULL,0LL,MAX_INT64,NULL,                                       "max. number of bytes to store on ftp server","unlimited"                  ),

  CMD_OPTION_INTEGER      ("ssh-port",                          0,  0,2,globalOptions.defaultSSHServer.ssh.port,                        0,0,65535,NULL,                                                "ssh port",NULL                                                            ),
  CMD_OPTION_STRING       ("ssh-login-name",                    0,  0,2,globalOptions.defaultSSHServer.ssh.loginName,                   0,                                                             "ssh login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ssh-password",                      0,  0,2,&globalOptions.defaultSSHServer.ssh.password,                   0,cmdOptionParsePassword,NULL,1,                               "ssh password (use with care!)","password"                                 ),
  CMD_OPTION_SPECIAL      ("ssh-public-key",                    0,  1,2,&globalOptions.defaultSSHServer.ssh.publicKey,                  0,cmdOptionParseKey,NULL,1,                                    "ssh public key","file name|data"                                          ),
  CMD_OPTION_SPECIAL      ("ssh-private-key",                   0,  1,2,&globalOptions.defaultSSHServer.ssh.privateKey,                 0,cmdOptionParseKey,NULL,1,                                    "ssh private key","file name|data"                                         ),
  CMD_OPTION_INTEGER      ("ssh-max-connections",               0,  1,2,globalOptions.defaultSSHServer.maxConnectionCount,              0,0,MAX_INT,NULL,                                              "max. number of concurrent ssh connections","unlimited"                    ),
//TODO
//  CMD_OPTION_INTEGER64    ("ssh-max-storage-size",              0,  0,2,defaultSSHServer.maxStorageSize,                  0,0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on ssh server","unlimited"                  ),

//  CMD_OPTION_INTEGER      ("webdav-port",                       0,  0,2,defaultWebDAVServer.webDAV.port,                0,0,65535,NULL,                                                  "WebDAV port",NULL                                                         ),
  CMD_OPTION_STRING       ("webdav-login-name",                 0,  0,2,globalOptions.defaultWebDAVServer.webDAV.loginName,             0,                                                             "WebDAV login name","name"                                                 ),
  CMD_OPTION_SPECIAL      ("webdav-password",                   0,  0,2,&globalOptions.defaultWebDAVServer.webDAV.password,             0,cmdOptionParsePassword,NULL,1,                               "WebDAV password (use with care!)","password"                              ),
  CMD_OPTION_INTEGER      ("webdav-max-connections",            0,  0,2,globalOptions.defaultWebDAVServer.maxConnectionCount,           0,0,MAX_INT,NULL,                                              "max. number of concurrent WebDAV connections","unlimited"                 ),
//TODO
//  CMD_OPTION_INTEGER64    ("webdav-max-storage-size",           0,  0,2,defaultWebDAVServer.maxStorageSize,               0,0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on WebDAV server","unlimited"               ),

  CMD_OPTION_BOOLEAN      ("daemon",                            0,  1,0,globalOptions.daemonFlag,                         0,                                                             "run in daemon mode"                                                       ),
  CMD_OPTION_BOOLEAN      ("no-detach",                         'D',1,0,globalOptions.noDetachFlag,                       0,                                                             "do not detach in daemon mode"                                             ),
//  CMD_OPTION_BOOLEAN      ("pair-master",                       0  ,1,0,pairMasterFlag,                                   0,                                                             "pair master"                                                              ),
  CMD_OPTION_SELECT       ("server-mode",                       0,  1,1,globalOptions.serverMode,                         0,COMMAND_LINE_OPTIONS_SERVER_MODES,             "select server mode"                                                       ),
  CMD_OPTION_INTEGER      ("server-port",                       0,  1,1,globalOptions.serverPort,                         0,0,65535,NULL,                                                "server port",NULL                                                         ),
  CMD_OPTION_INTEGER      ("server-tls-port",                   0,  1,1,globalOptions.serverTLSPort,                      0,0,65535,NULL,                                                "TLS (SSL) server port",NULL                                               ),
  CMD_OPTION_SPECIAL      ("server-ca-file",                    0,  1,1,&globalOptions.serverCA,                          0,cmdOptionReadCertificateFile,NULL,1,           "TLS (SSL) server certificate authority file (CA file)","file name"        ),
  CMD_OPTION_SPECIAL      ("server-cert-file",                  0,  1,1,&globalOptions.serverCert,                        0,cmdOptionReadCertificateFile,NULL,1,           "TLS (SSL) server certificate file","file name"                            ),
  CMD_OPTION_SPECIAL      ("server-key-file",                   0,  1,1,&globalOptions.serverKey,                         0,cmdOptionReadKeyFile,NULL,1,                   "TLS (SSL) server key file","file name"                                    ),
  CMD_OPTION_SPECIAL      ("server-password",                   0,  1,1,&globalOptions.serverPasswordHash,                0,cmdOptionParseHashData,NULL,1,                 "server password (use with care!)","password"                              ),
  CMD_OPTION_INTEGER      ("server-max-connections",            0,  1,1,globalOptions.serverMaxConnections,               0,0,65535,NULL,                                  "max. concurrent connections to server",NULL                               ),

  CMD_OPTION_INTEGER      ("nice-level",                        0,  1,1,globalOptions.niceLevel,                          0,0,19,NULL,                                                   "general nice level of processes/threads",NULL                             ),
  CMD_OPTION_INTEGER      ("max-threads",                       0,  1,1,globalOptions.maxThreads,                         0,0,65535,NULL,                                                "max. number of concurrent compress/encryption threads","cpu cores"        ),

  CMD_OPTION_SPECIAL      ("max-band-width",                    0,  1,1,&globalOptions.maxBandWidthList,                  0,cmdOptionParseBandWidth,NULL,1,                              "max. network band width to use [bits/s]","number or file name"            ),

  CMD_OPTION_BOOLEAN      ("batch",                             0,  2,1,globalOptions.batchFlag,                          0,                                                             "run in batch mode"                                                        ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",             0,  1,1,&globalOptions.remoteBARExecutable,               0,cmdOptionParseString,NULL,1,                                 "remote BAR executable","file name"                                        ),

  CMD_OPTION_STRING       ("pre-command",                       0,  1,1,globalOptions.preProcessScript,                   0,                                                             "pre-process command","command"                                            ),
  CMD_OPTION_STRING       ("post-command",                      0,  1,1,globalOptions.postProcessScript,                  0,                                                             "post-process command","command"                                           ),

  CMD_OPTION_STRING       ("file-write-pre-command",            0,  1,1,globalOptions.file.writePreProcessCommand,        0,                                                             "write file pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("file-write-post-command",           0,  1,1,globalOptions.file.writePostProcessCommand,       0,                                                             "write file post-process command","command"                                ),

  CMD_OPTION_STRING       ("ftp-write-pre-command",             0,  1,1,globalOptions.ftp.writePreProcessCommand,         0,                                                             "write FTP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("ftp-write-post-command",            0,  1,1,globalOptions.ftp.writePostProcessCommand,        0,                                                             "write FTP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("scp-write-pre-command",             0,  1,1,globalOptions.scp.writePreProcessCommand,         0,                                                             "write SCP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("scp-write-post-command",            0,  1,1,globalOptions.scp.writePostProcessCommand,        0,                                                             "write SCP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("sftp-write-pre-command",            0,  1,1,globalOptions.sftp.writePreProcessCommand,        0,                                                             "write SFTP pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("sftp-write-post-command",           0,  1,1,globalOptions.sftp.writePostProcessCommand,       0,                                                             "write SFTP post-process command","command"                                ),

  CMD_OPTION_STRING       ("webdav-write-pre-command",          0,  1,1,globalOptions.webdav.writePreProcessCommand,      0,                                                             "write WebDAV pre-process command","command"                               ),
  CMD_OPTION_STRING       ("webdav-write-post-command",         0,  1,1,globalOptions.webdav.writePostProcessCommand,     0,                                                             "write WebDAV post-process command","command"                              ),

  CMD_OPTION_STRING       ("cd-device",                         0,  1,1,globalOptions.cd.deviceName,                      0,                                                             "default CD device","device name"                                          ),
  CMD_OPTION_STRING       ("cd-request-volume-command",         0,  1,1,globalOptions.cd.requestVolumeCommand,            0,                                                             "request new CD volume command","command"                                  ),
  CMD_OPTION_STRING       ("cd-unload-volume-command",          0,  1,1,globalOptions.cd.unloadVolumeCommand,             0,                                                             "unload CD volume command","command"                                       ),
  CMD_OPTION_STRING       ("cd-load-volume-command",            0,  1,1,globalOptions.cd.loadVolumeCommand,               0,                                                             "load CD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("cd-volume-size",                    0,  1,1,globalOptions.cd.volumeSize,                      0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "CD volume size","unlimited"                                               ),
  CMD_OPTION_STRING       ("cd-image-pre-command",              0,  1,1,globalOptions.cd.imagePreProcessCommand,          0,                                                             "make CD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("cd-image-post-command",             0,  1,1,globalOptions.cd.imagePostProcessCommand,         0,                                                             "make CD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("cd-image-command",                  0,  1,1,globalOptions.cd.imageCommand,                    0,                                                             "make CD image command","command"                                          ),
  CMD_OPTION_STRING       ("cd-ecc-pre-command",                0,  1,1,globalOptions.cd.eccPreProcessCommand,            0,                                                             "make CD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("cd-ecc-post-command",               0,  1,1,globalOptions.cd.eccPostProcessCommand,           0,                                                             "make CD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("cd-ecc-command",                    0,  1,1,globalOptions.cd.eccCommand,                      0,                                                             "make CD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("cd-blank-command",                  0,  1,1,globalOptions.cd.blankCommand,                    0,                                                             "blank CD medium command","command"                                        ),
  CMD_OPTION_STRING       ("cd-write-pre-command",              0,  1,1,globalOptions.cd.writePreProcessCommand,          0,                                                             "write CD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("cd-write-post-command",             0,  1,1,globalOptions.cd.writePostProcessCommand,         0,                                                             "write CD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("cd-write-command",                  0,  1,1,globalOptions.cd.writeCommand,                    0,                                                             "write CD command","command"                                               ),
  CMD_OPTION_STRING       ("cd-write-image-command",            0,  1,1,globalOptions.cd.writeImageCommand,               0,                                                             "write CD image command","command"                                         ),

  CMD_OPTION_STRING       ("dvd-device",                        0,  1,1,globalOptions.dvd.deviceName,                     0,                                                             "default DVD device","device name"                                         ),
  CMD_OPTION_STRING       ("dvd-request-volume-command",        0,  1,1,globalOptions.dvd.requestVolumeCommand,           0,                                                             "request new DVD volume command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-unload-volume-command",         0,  1,1,globalOptions.dvd.unloadVolumeCommand,            0,                                                             "unload DVD volume command","command"                                      ),
  CMD_OPTION_STRING       ("dvd-load-volume-command",           0,  1,1,globalOptions.dvd.loadVolumeCommand,              0,                                                             "load DVD volume command","command"                                        ),
  CMD_OPTION_INTEGER64    ("dvd-volume-size",                   0,  1,1,globalOptions.dvd.volumeSize,                     0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "DVD volume size","unlimited"                                              ),
  CMD_OPTION_STRING       ("dvd-image-pre-command",             0,  1,1,globalOptions.dvd.imagePreProcessCommand,         0,                                                             "make DVD image pre-process command","command"                             ),
  CMD_OPTION_STRING       ("dvd-image-post-command",            0,  1,1,globalOptions.dvd.imagePostProcessCommand,        0,                                                             "make DVD image post-process command","command"                            ),
  CMD_OPTION_STRING       ("dvd-image-command",                 0,  1,1,globalOptions.dvd.imageCommand,                   0,                                                             "make DVD image command","command"                                         ),
  CMD_OPTION_STRING       ("dvd-ecc-pre-command",               0,  1,1,globalOptions.dvd.eccPreProcessCommand,           0,                                                             "make DVD error-correction codes pre-process command","command"            ),
  CMD_OPTION_STRING       ("dvd-ecc-post-command",              0,  1,1,globalOptions.dvd.eccPostProcessCommand,          0,                                                             "make DVD error-correction codes post-process command","command"           ),
  CMD_OPTION_STRING       ("dvd-ecc-command",                   0,  1,1,globalOptions.dvd.eccCommand,                     0,                                                             "make DVD error-correction codes command","command"                        ),
  CMD_OPTION_STRING       ("dvd-blank-command",                 0,  1,1,globalOptions.dvd.blankCommand,                   0,                                                             "blank DVD mediumcommand","command"                                        ),
  CMD_OPTION_STRING       ("dvd-write-pre-command",             0,  1,1,globalOptions.dvd.writePreProcessCommand,         0,                                                             "write DVD pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("dvd-write-post-command",            0,  1,1,globalOptions.dvd.writePostProcessCommand,        0,                                                             "write DVD post-process command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-write-command",                 0,  1,1,globalOptions.dvd.writeCommand,                   0,                                                             "write DVD command","command"                                              ),
  CMD_OPTION_STRING       ("dvd-write-image-command",           0,  1,1,globalOptions.dvd.writeImageCommand,              0,                                                             "write DVD image command","command"                                        ),

  CMD_OPTION_STRING       ("bd-device",                         0,  1,1,globalOptions.bd.deviceName,                      0,                                                             "default BD device","device name"                                          ),
  CMD_OPTION_STRING       ("bd-request-volume-command",         0,  1,1,globalOptions.bd.requestVolumeCommand,            0,                                                             "request new BD volume command","command"                                  ),
  CMD_OPTION_STRING       ("bd-unload-volume-command",          0,  1,1,globalOptions.bd.unloadVolumeCommand,             0,                                                             "unload BD volume command","command"                                       ),
  CMD_OPTION_STRING       ("bd-load-volume-command",            0,  1,1,globalOptions.bd.loadVolumeCommand,               0,                                                             "load BD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("bd-volume-size",                    0,  1,1,globalOptions.bd.volumeSize,                      0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "BD volume size","unlimited"                                               ),
  CMD_OPTION_STRING       ("bd-image-pre-command",              0,  1,1,globalOptions.bd.imagePreProcessCommand,          0,                                                             "make BD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("bd-image-post-command",             0,  1,1,globalOptions.bd.imagePostProcessCommand,         0,                                                             "make BD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("bd-image-command",                  0,  1,1,globalOptions.bd.imageCommand,                    0,                                                             "make BD image command","command"                                          ),
  CMD_OPTION_STRING       ("bd-ecc-pre-command",                0,  1,1,globalOptions.bd.eccPreProcessCommand,            0,                                                             "make BD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("bd-ecc-post-command",               0,  1,1,globalOptions.bd.eccPostProcessCommand,           0,                                                             "make BD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("bd-ecc-command",                    0,  1,1,globalOptions.bd.eccCommand,                      0,                                                             "make BD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("bd-blank-command",                  0,  1,1,globalOptions.bd.blankCommand,                    0,                                                             "blank BD medium command","command"                                        ),
  CMD_OPTION_STRING       ("bd-write-pre-command",              0,  1,1,globalOptions.bd.writePreProcessCommand,          0,                                                             "write BD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("bd-write-post-command",             0,  1,1,globalOptions.bd.writePostProcessCommand,         0,                                                             "write BD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("bd-write-command",                  0,  1,1,globalOptions.bd.writeCommand,                    0,                                                             "write BD command","command"                                               ),
  CMD_OPTION_STRING       ("bd-write-image-command",            0,  1,1,globalOptions.bd.writeImageCommand,               0,                                                             "write BD image command","command"                                         ),

  CMD_OPTION_STRING       ("device",                            0,  1,1,globalOptions.defaultDevice.name,                               0,                                                             "default device","device name"                                             ),
  CMD_OPTION_STRING       ("device-request-volume-command",     0,  1,1,globalOptions.defaultDevice.requestVolumeCommand,               0,                                                             "request new volume command","command"                                     ),
  CMD_OPTION_STRING       ("device-load-volume-command",        0,  1,1,globalOptions.defaultDevice.loadVolumeCommand,                  0,                                                             "load volume command","command"                                            ),
  CMD_OPTION_STRING       ("device-unload-volume-command",      0,  1,1,globalOptions.defaultDevice.unloadVolumeCommand,                0,                                                             "unload volume command","command"                                          ),
  CMD_OPTION_INTEGER64    ("device-volume-size",                0,  1,1,globalOptions.defaultDevice.volumeSize,                         0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "volume size","unlimited"                                                  ),
  CMD_OPTION_STRING       ("device-image-pre-command",          0,  1,1,globalOptions.defaultDevice.imagePreProcessCommand,             0,                                                             "make image pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("device-image-post-command",         0,  1,1,globalOptions.defaultDevice.imagePostProcessCommand,            0,                                                             "make image post-process command","command"                                ),
  CMD_OPTION_STRING       ("device-image-command",              0,  1,1,globalOptions.defaultDevice.imageCommand,                       0,                                                             "make image command","command"                                             ),
  CMD_OPTION_STRING       ("device-ecc-pre-command",            0,  1,1,globalOptions.defaultDevice.eccPreProcessCommand,               0,                                                             "make error-correction codes pre-process command","command"                ),
  CMD_OPTION_STRING       ("device-ecc-post-command",           0,  1,1,globalOptions.defaultDevice.eccPostProcessCommand,              0,                                                             "make error-correction codes post-process command","command"               ),
  CMD_OPTION_STRING       ("device-ecc-command",                0,  1,1,globalOptions.defaultDevice.eccCommand,                         0,                                                             "make error-correction codes command","command"                            ),
  CMD_OPTION_STRING       ("device-blank-command",              0,  1,1,globalOptions.defaultDevice.blankCommand,                       0,                                                             "blank device medium command","command"                                    ),
  CMD_OPTION_STRING       ("device-write-pre-command",          0,  1,1,globalOptions.defaultDevice.writePreProcessCommand,             0,                                                             "write device pre-process command","command"                               ),
  CMD_OPTION_STRING       ("device-write-post-command",         0,  1,1,globalOptions.defaultDevice.writePostProcessCommand,            0,                                                             "write device post-process command","command"                              ),
  CMD_OPTION_STRING       ("device-write-command",              0,  1,1,globalOptions.defaultDevice.writeCommand,                       0,                                                             "write device command","command"                                           ),

  CMD_OPTION_INTEGER64    ("max-storage-size",                  0,  1,2,globalOptions.maxStorageSize,                     0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "max. storage size","unlimited"                                            ),
  CMD_OPTION_INTEGER64    ("volume-size",                       0,  1,2,globalOptions.volumeSize,                         0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "volume size","unlimited"                                                  ),
  CMD_OPTION_BOOLEAN      ("ecc",                               0,  1,2,globalOptions.errorCorrectionCodesFlag,           0,                                                             "add error-correction codes with 'dvdisaster' tool"                        ),
  CMD_OPTION_BOOLEAN      ("always-create-image",               0,  1,2,globalOptions.alwaysCreateImageFlag,              0,                                                             "always create image for CD/DVD/BD/device"                                 ),
  CMD_OPTION_BOOLEAN      ("blank",                             0,  1,2,globalOptions.blankFlag,                          0,                                                             "blank medium before writing"                                              ),

  CMD_OPTION_STRING       ("jobs-directory",                    0,  1,1,globalOptions.jobsDirectory,                      0,                                                             "server job directory","path name"                                         ),
  CMD_OPTION_STRING       ("incremental-data-directory",        0,  1,1,globalOptions.incrementalDataDirectory,           0,                                                             "server incremental data directory","path name"                            ),

  CMD_OPTION_CSTRING      ("index-database",                    0,  1,1,globalOptions.indexDatabaseFileName,              0,                                                             "index database file name","file name"                                     ),
  CMD_OPTION_BOOLEAN      ("index-database-update",             0,  1,1,globalOptions.indexDatabaseUpdateFlag,            0,                                                             "enabled update index database"                                            ),
  CMD_OPTION_BOOLEAN      ("index-database-auto-update",        0,  1,1,globalOptions.indexDatabaseAutoUpdateFlag,        0,                                                             "enabled automatic update index database"                                  ),
  CMD_OPTION_SPECIAL      ("index-database-max-band-width",     0,  1,1,&globalOptions.indexDatabaseMaxBandWidthList,     0,cmdOptionParseBandWidth,NULL,1,                              "max. band width to use for index updates [bis/s]","number or file name"   ),
  CMD_OPTION_INTEGER      ("index-database-keep-time",          0,  1,1,globalOptions.indexDatabaseKeepTime,              0,0,MAX_INT,COMMAND_LINE_TIME_UNITS,                           "time to keep index data of not existing storages",NULL                    ),

  CMD_OPTION_CSTRING      ("continuous-database",               0,  2,1,globalOptions.continuousDatabaseFileName,         0,                                                             "continuous database file name (default: in memory)","file name"           ),
  CMD_OPTION_INTEGER64    ("continuous-max-size",               0,  1,2,globalOptions.continuousMaxSize,                  0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "max. continuous size","unlimited"                                         ),
  CMD_OPTION_INTEGER      ("continuous-min-time-delta",         0,  1,1,globalOptions.continuousMinTimeDelta,             0,0,MAX_INT,COMMAND_LINE_TIME_UNITS,                           "min. time between continuous backup of an entry",NULL                     ),

  CMD_OPTION_SET          ("log",                               0,  1,1,globalOptions.logTypes,                           0,COMMAND_LINE_OPTIONS_LOG_TYPES,                              "log types"                                                                ),
  CMD_OPTION_CSTRING      ("log-file",                          0,  1,1,globalOptions.logFileName,                        0,                                                             "log file name","file name"                                                ),
  CMD_OPTION_CSTRING      ("log-format",                        0,  1,1,globalOptions.logFormat,                          0,                                                             "log format","format"                                                      ),
  CMD_OPTION_CSTRING      ("log-post-command",                  0,  1,1,globalOptions.logPostCommand,                     0,                                                             "log file post-process command","command"                                  ),

  CMD_OPTION_CSTRING      ("pid-file",                          0,  1,1,globalOptions.pidFileName,                        0,                                                             "process id file name","file name"                                         ),

  CMD_OPTION_CSTRING      ("pairing-master-file",               0,  1,1,globalOptions.masterInfo.pairingFileName,         0,                                                             "pairing master enable file name","file name"                              ),

  CMD_OPTION_BOOLEAN      ("info",                              0  ,0,1,globalOptions.metaInfoFlag,                       0,                                                             "show meta info"                                                           ),

  CMD_OPTION_BOOLEAN      ("group",                             'g',0,1,globalOptions.groupFlag,                          0,                                                             "group files in list"                                                      ),
  CMD_OPTION_BOOLEAN      ("all",                               0,  0,1,globalOptions.allFlag,                            0,                                                             "show all files"                                                           ),
  CMD_OPTION_BOOLEAN      ("long-format",                       'L',0,1,globalOptions.longFormatFlag,                     0,                                                             "list in long format"                                                      ),
  CMD_OPTION_BOOLEAN      ("human-format",                      'H',0,1,globalOptions.humanFormatFlag,                    0,                                                             "list in human readable format"                                            ),
  CMD_OPTION_BOOLEAN      ("numeric-uid-gid",                   0,  0,1,globalOptions.numericUIDGIDFlag,                  0,                                                             "print numeric user/group ids"                                             ),
  CMD_OPTION_BOOLEAN      ("numeric-permissions",               0,  0,1,globalOptions.numericPermissionsFlag,             0,                                                             "print numeric file/directory permissions"                                 ),
  CMD_OPTION_BOOLEAN      ("no-header-footer",                  0,  0,1,globalOptions.noHeaderFooterFlag,                 0,                                                             "no header/footer output in list"                                          ),
  CMD_OPTION_BOOLEAN      ("delete-old-archive-files",          0,  1,1,globalOptions.deleteOldArchiveFilesFlag,          0,                                                             "delete old archive files after creating new files"                        ),
  CMD_OPTION_BOOLEAN      ("ignore-no-backup-file",             0,  1,2,globalOptions.ignoreNoBackupFileFlag,             0,                                                             "ignore .nobackup/.NOBACKUP file"                                          ),
  CMD_OPTION_BOOLEAN      ("ignore-no-dump",                    0,  1,2,globalOptions.ignoreNoDumpAttributeFlag,          0,                                                             "ignore 'no dump' attribute of files"                                      ),

  CMD_OPTION_BOOLEAN      ("skip-unreadable",                   0,  0,2,globalOptions.skipUnreadableFlag,                 0,                                                             "skip unreadable files"                                                    ),
  CMD_OPTION_BOOLEAN      ("force-delta-compression",           0,  0,2,globalOptions.forceDeltaCompressionFlag,          0,                                                             "force delta compression of files. Stop on error"                          ),
  CMD_OPTION_BOOLEAN      ("raw-images",                        0,  1,2,globalOptions.rawImagesFlag,                      0,                                                             "store raw images (store all image blocks)"                                ),
  CMD_OPTION_BOOLEAN      ("no-fragments-check",                0,  1,2,globalOptions.noFragmentsCheckFlag,               0,                                                             "do not check completeness of file fragments"                              ),
  CMD_OPTION_BOOLEAN      ("no-index-database",                 0,  1,1,globalOptions.noIndexDatabaseFlag,                0,                                                             "do not store index database for archives"                                 ),
  CMD_OPTION_SELECT       ("archive-file-mode",                 0,  1,2,globalOptions.archiveFileMode,                    0,COMMAND_LINE_OPTIONS_ARCHIVE_FILE_MODES,                     "select archive files write mode"                                          ),
  // Note: shortcut for --archive-file-mode=overwrite
  CMD_OPTION_SPECIAL      ("overwrite-archive-files",           'o',0,2,&globalOptions.archiveFileMode,                   0,cmdOptionParseArchiveFileModeOverwrite,NULL,0,               "overwrite existing archive files",""                                      ),
  CMD_OPTION_SELECT       ("restore-entry-mode",                0,  1,2,globalOptions.restoreEntryMode,                   0,COMMAND_LINE_OPTIONS_RESTORE_ENTRY_MODES,                    "restore entry mode"                                                       ),
  // Note: shortcut for --restore-entry-mode=overwrite
  CMD_OPTION_SPECIAL      ("overwrite-files",                   0,  0,2,&globalOptions.restoreEntryMode,                  0,cmdOptionParseRestoreEntryModeOverwrite,NULL,0,              "overwrite existing entries on restore",""                                 ),
  CMD_OPTION_BOOLEAN      ("wait-first-volume",                 0,  1,2,globalOptions.waitFirstVolumeFlag,                0,                                                             "wait for first volume"                                                    ),
  CMD_OPTION_BOOLEAN      ("no-signature",                      0  ,1,2,globalOptions.noSignatureFlag,                    0,                                                             "do not create signatures"                                                 ),
  CMD_OPTION_BOOLEAN      ("skip-verify-signatures",            0,  0,2,globalOptions.skipVerifySignaturesFlag,           0,                                                             "do not verify signatures of archives"                                     ),
  CMD_OPTION_BOOLEAN      ("force-verify-signatures",           0,  0,2,globalOptions.forceVerifySignaturesFlag,          0,                                                             "force verify signatures of archives. Stop on error"                       ),
  CMD_OPTION_BOOLEAN      ("no-bar-on-medium",                  0,  1,2,globalOptions.noBAROnMediumFlag,                  0,                                                             "do not store a copy of BAR on medium"                                     ),
  CMD_OPTION_BOOLEAN      ("no-stop-on-error",                  0,  1,2,globalOptions.noStopOnErrorFlag,                  0,                                                             "do not immediately stop on error"                                         ),
  CMD_OPTION_BOOLEAN      ("no-stop-on-attribute-error",        0,  1,2,globalOptions.noStopOnAttributeErrorFlag,         0,                                                             "do not immediately stop on attribute error"                               ),

  CMD_OPTION_SPECIAL      ("no-storage",                        0,  1,2,&globalOptions.storageFlags,                      0,cmdOptionParseStorageFlagNoStorage,NULL,0,                   "do not store archives (skip storage, index database",""                   ),
  CMD_OPTION_SPECIAL      ("dry-run",                           0,  1,2,&globalOptions.storageFlags,                      0,cmdOptionParseStorageFlagDryRun,NULL,0,                      "do dry-run (skip storage/restore, incremental data, index database)",""   ),

  CMD_OPTION_BOOLEAN      ("no-default-config",                 0,  1,1,globalOptions.noDefaultConfigFlag,                0,                                                             "do not read configuration files " CONFIG_DIR "/bar.cfg and ~/.bar/" DEFAULT_CONFIG_FILE_NAME),
  CMD_OPTION_BOOLEAN      ("quiet",                             0,  1,1,globalOptions.quietFlag,                          0,                                                             "suppress any output"                                                      ),
  CMD_OPTION_INCREMENT    ("verbose",                           'v',0,0,globalOptions.verboseLevel,                       0,0,6,                                                         "increment/set verbosity level"                                            ),

  CMD_OPTION_BOOLEAN      ("version",                           0  ,0,0,globalOptions.versionFlag,                        0,                                                             "output version"                                                           ),
  CMD_OPTION_BOOLEAN      ("help",                              'h',0,0,globalOptions.helpFlag,                           0,                                                             "output this help"                                                         ),
  CMD_OPTION_BOOLEAN      ("xhelp",                             0,  0,0,globalOptions.xhelpFlag,                          0,                                                             "output help to extended options"                                          ),
  CMD_OPTION_BOOLEAN      ("help-internal",                     0,  1,0,globalOptions.helpInternalFlag,                   0,                                                             "output help to internal options"                                          ),

  // deprecated
  CMD_OPTION_DEPRECATED   ("server-jobs-directory",             0,  1,1,&globalOptions.jobsDirectory,                     0,CmdOptionParseDeprecatedStringOption,NULL,1,                 "jobs-directory"                                                           ),
  CMD_OPTION_DEPRECATED   ("mount-device",                      0,  1,2,&globalOptions.mountList,                         0,cmdOptionParseDeprecatedMountDevice,NULL,1,                  "device to mount/unmount"                                                  ),
  CMD_OPTION_DEPRECATED   ("stop-on-error",                     0,  1,2,&globalOptions.noStopOnErrorFlag,                 0,cmdOptionParseDeprecatedStopOnError,NULL,0,                  "no-stop-on-error"                                                         ),

  CMD_OPTION_INCREMENT    ("debug-server",                      0,  2,1,globalOptions.debug.serverLevel,                  0,0,2,                                                         "debug level for server"                                                   ),
  CMD_OPTION_BOOLEAN      ("debug-server-fixed-ids",            0,  2,1,globalOptions.debug.serverFixedIdsFlag,           0,                                                             "fixed server ids"                                                         ),
  CMD_OPTION_BOOLEAN      ("debug-index-wait-operations",       0,  2,1,globalOptions.debug.indexWaitOperationsFlag,      0,                                                             "wait for index operations"                                                ),
  CMD_OPTION_BOOLEAN      ("debug-index-purge-deleted-storages",0,  2,1,globalOptions.debug.indexPurgeDeletedStoragesFlag,0,                                                             "wait for index operations"                                                ),
  CMD_OPTION_STRING       ("debug-index-add-storage",           0,  2,1,globalOptions.debug.indexAddStorage,              0,                                                             "add storage to index database","file name"                                ),
  CMD_OPTION_STRING       ("debug-index-remove-storage",        0,  2,1,globalOptions.debug.indexRemoveStorage,           0,                                                             "remove storage from index database","file name"                           ),
  CMD_OPTION_STRING       ("debug-index-refresh-storage",       0,  2,1,globalOptions.debug.indexRefreshStorage,          0,                                                             "refresh storage in index database","file name"                            ),
);

ConfigValue CONFIG_VALUES[] = CONFIG_VALUE_ARRAY
(
  CONFIG_VALUE_SEPARATOR(),
  CONFIG_VALUE_COMMENT("BAR configuration"),
  CONFIG_VALUE_SEPARATOR(),
  CONFIG_VALUE_SPACE(),

  // general settings
  CONFIG_VALUE_STRING            ("UUID",                             &uuid,-1,                                                      "<uuid>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("additional configuration files"),
  CONFIG_VALUE_SPECIAL           ("config",                           &configFileList,-1,                                            configValueConfigFileParse,configValueConfigFileFormat,NULL),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("master settings"),
  CONFIG_VALUE_BEGIN_SECTION     ("master",NULL,-1,NULL,NULL,NULL,NULL),
//TODO
    CONFIG_VALUE_STRING          ("name",                             &globalOptions.masterInfo.name,-1,                             "<name>"),
    CONFIG_VALUE_SPECIAL         ("uuid-hash",                        &globalOptions.masterInfo.uuidHash,-1,                         configValueHashDataParse,configValueHashDataFormat,NULL),
//TODO: required to save?
    CONFIG_VALUE_COMMENT("pubic key"),
    CONFIG_VALUE_SPECIAL         ("public-key",                       &globalOptions.masterInfo.publicKey,-1,                        configValueKeyParse,configValueKeyFormat,NULL),
  CONFIG_VALUE_END_SECTION(),
  CONFIG_VALUE_COMMENT           ("pairing master trigger/clear file"),
  CONFIG_VALUE_CSTRING           ("pairing-master-file",              &globalOptions.masterInfo.pairingFileName,-1,                  "<file name>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("temporary directory"),
  CONFIG_VALUE_STRING            ("tmp-directory",                    &globalOptions.tmpDirectory,-1,                                "<directory>"),
  CONFIG_VALUE_COMMENT("max. temporary space to use"),
  CONFIG_VALUE_INTEGER64         ("max-tmp-size",                     &globalOptions.maxTmpSize,-1,                                  0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS,"<size>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("worker thread nice level"),
  CONFIG_VALUE_INTEGER           ("nice-level",                       &globalOptions.niceLevel,-1,                                   0,19,NULL,"<level>"),
  CONFIG_VALUE_COMMENT("max. number of worker threads"),
  CONFIG_VALUE_INTEGER           ("max-threads",                      &globalOptions.maxThreads,-1,                                  0,65535,NULL,"<n>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("max. network band width to use"),
  CONFIG_VALUE_SPECIAL           ("max-band-width",                   &globalOptions.maxBandWidthList,-1,                            configValueBandWidthParse,configValueBandWidthFormat,NULL),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("jobs directory"),
  CONFIG_VALUE_STRING            ("jobs-directory",                   &globalOptions.jobsDirectory,-1,                               "<directory>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("incremental data directory"),
  CONFIG_VALUE_STRING            ("incremental-data-directory",       &globalOptions.incrementalDataDirectory,-1,                    "<directory>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_SEPARATOR("index database"),
  CONFIG_VALUE_CSTRING           ("index-database",                   &globalOptions.indexDatabaseFileName,-1,                       "<file name>"),
  CONFIG_VALUE_BOOLEAN           ("index-database-update",            &globalOptions.indexDatabaseUpdateFlag,-1,                     "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("index-database-auto-update",       &globalOptions.indexDatabaseAutoUpdateFlag,-1,                 "yes|no"),
  CONFIG_VALUE_SPECIAL           ("index-database-max-band-width",    &globalOptions.indexDatabaseMaxBandWidthList,-1,               configValueBandWidthParse,configValueBandWidthFormat,NULL),
  CONFIG_VALUE_INTEGER           ("index-database-keep-time",         &globalOptions.indexDatabaseKeepTime,-1,                       0,MAX_INT,CONFIG_VALUE_TIME_UNITS,"<n>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_SEPARATOR("continous meta database"),
  CONFIG_VALUE_CSTRING           ("continuous-database",              &globalOptions.continuousDatabaseFileName,-1,                  "<file name>"),
  CONFIG_VALUE_INTEGER64         ("continuous-max-size",              &globalOptions.continuousMaxSize,-1,                           0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_VALUE_INTEGER           ("continuous-min-time-delta",        &globalOptions.continuousMinTimeDelta,-1,                      0,MAX_INT,CONFIG_VALUE_TIME_UNITS,"<n>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_SEPARATOR("maintenance"),
  CONFIG_VALUE_BEGIN_SECTION     ("maintenance",&globalOptions.maintenanceList,-1,ConfigValue_listSectionDataIteratorInit,ConfigValue_listSectionDataIteratorDone,ConfigValue_listSectionDataIteratorNext,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL  ("date",                             MaintenanceNode,date,                                          configValueMaintenanceDateParse,configValueMaintenanceDateFormat,&globalOptions.maintenanceList),
    CONFIG_STRUCT_VALUE_SPECIAL  ("weekdays",                         MaintenanceNode,weekDaySet,                                    configValueMaintenanceWeekDaySetParse,configValueMaintenanceWeekDaySetFormat,&globalOptions.maintenanceList),
    CONFIG_STRUCT_VALUE_SPECIAL  ("begin",                            MaintenanceNode,beginTime,                                     configValueMaintenanceTimeParse,configValueMaintenanceTimeFormat,&globalOptions.maintenanceList),
    CONFIG_STRUCT_VALUE_SPECIAL  ("end",                              MaintenanceNode,endTime,                                       configValueMaintenanceTimeParse,configValueMaintenanceTimeFormat,&globalOptions.maintenanceList),
  CONFIG_VALUE_END_SECTION(),

  // global job settings
  CONFIG_VALUE_COMMENT("archive"),
  CONFIG_VALUE_IGNORE            ("host-name",                                                                                       NULL,FALSE),
  CONFIG_VALUE_IGNORE            ("host-port",                                                                                       NULL,FALSE),
  CONFIG_VALUE_STRING            ("archive-name",                     &globalOptions.storageName,-1,                                 "<file name>"),
  CONFIG_VALUE_SELECT            ("archive-type",                     &globalOptions.archiveType,-1,                                 CONFIG_VALUE_ARCHIVE_TYPES,"<type>"),

  CONFIG_VALUE_COMMENT("incremental list file"),
  CONFIG_VALUE_STRING            ("incremental-list-file",            &globalOptions.incrementalListFileName,-1,                     "<file name>"),

  CONFIG_VALUE_COMMENT("archive part size"),
  CONFIG_VALUE_INTEGER64         ("archive-part-size",                &globalOptions.archivePartSize,-1,                             0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS,"<size>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("directory count to strip on restore"),
  CONFIG_VALUE_INTEGER           ("directory-strip",                  &globalOptions.directoryStripCount,-1,                         -1,MAX_INT,NULL,"<n>"),
  CONFIG_VALUE_COMMENT("destination diretory for restore"),
  CONFIG_VALUE_STRING            ("destination",                      &globalOptions.destination,-1,                                 "<directory>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("owner/permission for restore"),
  CONFIG_VALUE_SPECIAL           ("owner",                            &globalOptions.owner,-1,                                       configValueOwnerParse,configValueOwnerFormat,NULL),
  CONFIG_VALUE_SPECIAL           ("permissions",                      &globalOptions.permissions,-1,                                 configValuePermissionsParse,configValuePermissionsFormat,NULL),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_SELECT            ("pattern-type",                     &globalOptions.patternType,-1,                                 CONFIG_VALUE_PATTERN_TYPES,"<type>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("compress algorithm"),
  CONFIG_VALUE_SPECIAL           ("compress-algorithm",               &globalOptions.compressAlgorithms,-1,                          configValueCompressAlgorithmsParse,configValueCompressAlgorithmsFormat,NULL),
  CONFIG_VALUE_INTEGER           ("compress-min-size",                &globalOptions.compressMinFileSize,-1,                         0,MAX_INT,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_VALUE_SPECIAL           ("compress-exclude",                 &globalOptions.compressExcludePatternList,-1,                  configValuePatternParse,configValuePatternFormat,NULL),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("crypt"),
  CONFIG_VALUE_SPECIAL           ("crypt-algorithm",                  &globalOptions.cryptAlgorithms,-1,                             configValueCryptAlgorithmsParse,configValueCryptAlgorithmsFormat,NULL),
  CONFIG_VALUE_SELECT            ("crypt-type",                       &globalOptions.cryptType,-1,                                   CONFIG_VALUE_CRYPT_TYPES,"<type>"),
  CONFIG_VALUE_SELECT            ("crypt-password-mode",              &globalOptions.cryptPasswordMode,-1,                           CONFIG_VALUE_PASSWORD_MODES,"<mode>"),
  CONFIG_VALUE_SPECIAL           ("crypt-password",                   &globalOptions.cryptPassword,-1,                               configValuePasswordParse,configValuePasswordFormat,NULL),
  CONFIG_VALUE_SPECIAL           ("crypt-public-key",                 &globalOptions.cryptPublicKey,-1,                              configValueKeyParse,configValueKeyFormat,NULL),
  CONFIG_VALUE_SPECIAL           ("crypt-private-key",                &globalOptions.cryptPrivateKey,-1,                             configValueKeyParse,configValueKeyFormat,NULL),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("signature keys"),
  CONFIG_VALUE_SPECIAL           ("signature-public-key",             &globalOptions.signaturePublicKey,-1,                          configValueKeyParse,configValueKeyFormat,NULL),
  CONFIG_VALUE_SPECIAL           ("signature-private-key",            &globalOptions.signaturePrivateKey,-1,                         configValueKeyParse,configValueKeyFormat,NULL),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("includes"),
  CONFIG_VALUE_SPECIAL           ("include-file",                     &globalOptions.includeEntryList,-1,                            configValueFileEntryPatternParse,configValueFileEntryPatternFormat,&globalOptions.patternType),
  CONFIG_VALUE_STRING            ("include-file-list",                &globalOptions.includeFileListFileName,-1,                     "<file name>"),
  CONFIG_VALUE_STRING            ("include-file-command",             &globalOptions.includeFileCommand,-1,                          "<command>"),
  CONFIG_VALUE_SPECIAL           ("include-image",                    &globalOptions.includeEntryList,-1,                            configValueImageEntryPatternParse,configValueImageEntryPatternFormat,&globalOptions.patternType),
  CONFIG_VALUE_STRING            ("include-image-list",               &globalOptions.includeImageListFileName,-1,                    "<file name>"),
  CONFIG_VALUE_STRING            ("include-image-command",            &globalOptions.includeImageCommand,-1,                         "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("excludes"),
  CONFIG_VALUE_SPECIAL           ("exclude",                          &globalOptions.excludePatternList,-1,                          configValuePatternParse,configValuePatternFormat,&globalOptions.patternType),
  CONFIG_VALUE_STRING            ("exclude-list",                     &globalOptions.excludeListFileName,-1,                         "<file name>"),
  CONFIG_VALUE_STRING            ("exclude-command",                  &globalOptions.excludeCommand,-1,                              "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT("mount"),
  CONFIG_VALUE_SPECIAL           ("mount",                            &globalOptions.mountList,-1,                                   configValueMountParse,configValueMountFormat,NULL),
  CONFIG_VALUE_STRING            ("mount-command",                    &globalOptions.mountCommand,-1,                                "<command>"),
  CONFIG_VALUE_STRING            ("mount-device-command",             &globalOptions.mountDeviceCommand,-1,                          "<command>"),
  CONFIG_VALUE_STRING            ("unmount-command",                  &globalOptions.unmountCommand,-1,                              "<command>"),

  CONFIG_VALUE_COMMENT("comment to add into archives"),
  CONFIG_VALUE_STRING            ("comment",                          &globalOptions.comment,-1,                                     "<text>"),

  CONFIG_VALUE_SPECIAL           ("delta-source",                     &globalOptions.deltaSourceList,-1,                             configValueDeltaSourceParse,configValueDeltaSourceFormat,NULL),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_INTEGER64         ("max-storage-size",                 &globalOptions.maxStorageSize,-1,                              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_VALUE_COMMENT("size of CV/DVD/BD volume"),
  CONFIG_VALUE_INTEGER64         ("volume-size",                      &globalOptions.volumeSize,-1,                                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_VALUE_COMMENT("enable error correction codes on CV/DVD/BD"),
  CONFIG_VALUE_BOOLEAN           ("ecc",                              &globalOptions.errorCorrectionCodesFlag,-1,                    "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("always-create-image",              &globalOptions.alwaysCreateImageFlag,-1,                       "yes|no"),
  CONFIG_VALUE_COMMENT("blank CV/DVD/BD before write"),
  CONFIG_VALUE_BOOLEAN           ("blank",                            &globalOptions.blankFlag,-1,                                   "yes|no"),

  CONFIG_VALUE_SPACE(),

//TODO remove
#if 0
  // ignored schedule settings (server only)
  CONFIG_VALUE_SEPARATOR("schedule"),
  CONFIG_VALUE_BEGIN_SECTION     ("schedule",NULL,-1,NULL,NULL,NULL,NULL),
    CONFIG_VALUE_IGNORE          ("UUID",                                                                                            NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("parentUUID",                                                                                      NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("date",                                                                                            NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("weekdays",                                                                                        NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("time",                                                                                            NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("archive-type",                                                                                    NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("interval",                                                                                        NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("text",                                                                                            NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("min-keep",                                                                                        NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("max-keep",                                                                                        NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("max-age",                                                                                         NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("enabled",                                                                                         NULL,FALSE),
  CONFIG_VALUE_END_SECTION(),

  // ignored persitence settings (server only)
  CONFIG_VALUE_SEPARATOR("persistence"),
  CONFIG_VALUE_BEGIN_SECTION     ("persistence",NULL,-1,NULL,NULL,NULL,NULL),
    CONFIG_VALUE_IGNORE          ("min-keep",                                                                                        NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("max-keep",                                                                                        NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("max-age",                                                                                         NULL,FALSE),
  CONFIG_VALUE_END_SECTION(),
#endif

  // commands
  CONFIG_VALUE_SEPARATOR("commands"),

  CONFIG_VALUE_STRING            ("pre-command",                      &globalOptions.preProcessScript,-1,                            "<command>"),
  CONFIG_VALUE_STRING            ("post-command",                     &globalOptions.postProcessScript,-1,                           "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_STRING            ("file-write-pre-command",           &globalOptions.file.writePreProcessCommand,-1,                 "<command>"),
  CONFIG_VALUE_STRING            ("file-write-post-command",          &globalOptions.file.writePostProcessCommand,-1,                "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_STRING            ("ftp-write-pre-command",            &globalOptions.ftp.writePreProcessCommand,-1,                  "<command>"),
  CONFIG_VALUE_STRING            ("ftp-write-post-command",           &globalOptions.ftp.writePostProcessCommand,-1,                 "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_STRING            ("scp-write-pre-command",            &globalOptions.scp.writePreProcessCommand,-1,                  "<command>"),
  CONFIG_VALUE_STRING            ("scp-write-post-command",           &globalOptions.scp.writePostProcessCommand,-1,                 "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_STRING            ("sftp-write-pre-command",           &globalOptions.sftp.writePreProcessCommand,-1,                 "<command>"),
  CONFIG_VALUE_STRING            ("sftp-write-post-command",          &globalOptions.sftp.writePostProcessCommand,-1,                "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_STRING            ("webdav-write-pre-command",         &globalOptions.webdav.writePreProcessCommand,-1,               "<command>"),
  CONFIG_VALUE_STRING            ("webdav-write-post-command",        &globalOptions.webdav.writePostProcessCommand,-1,              "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_STRING            ("cd-device",                        &globalOptions.cd.deviceName,-1,                               "<command>"),
  CONFIG_VALUE_STRING            ("cd-request-volume-command",        &globalOptions.cd.requestVolumeCommand,-1,                     "<command>"),
  CONFIG_VALUE_STRING            ("cd-unload-volume-command",         &globalOptions.cd.unloadVolumeCommand,-1,                      "<command>"),
  CONFIG_VALUE_STRING            ("cd-load-volume-command",           &globalOptions.cd.loadVolumeCommand,-1,                        "<command>"),
  CONFIG_VALUE_INTEGER64         ("cd-volume-size",                   &globalOptions.cd.volumeSize,-1,                               0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_VALUE_STRING            ("cd-image-pre-command",             &globalOptions.cd.imagePreProcessCommand,-1,                   "<command>"),
  CONFIG_VALUE_STRING            ("cd-image-post-command",            &globalOptions.cd.imagePostProcessCommand,-1,                  "<command>"),
  CONFIG_VALUE_STRING            ("cd-image-command",                 &globalOptions.cd.imageCommand,-1,                             "<command>"),
  CONFIG_VALUE_STRING            ("cd-ecc-pre-command",               &globalOptions.cd.eccPreProcessCommand,-1,                     "<command>"),
  CONFIG_VALUE_STRING            ("cd-ecc-post-command",              &globalOptions.cd.eccPostProcessCommand,-1,                    "<command>"),
  CONFIG_VALUE_STRING            ("cd-ecc-command",                   &globalOptions.cd.eccCommand,-1,                               "<command>"),
  CONFIG_VALUE_STRING            ("cd-blank-command",                 &globalOptions.cd.blankCommand,-1,                             "<command>"),
  CONFIG_VALUE_STRING            ("cd-write-pre-command",             &globalOptions.cd.writePreProcessCommand,-1,                   "<command>"),
  CONFIG_VALUE_STRING            ("cd-write-post-command",            &globalOptions.cd.writePostProcessCommand,-1,                  "<command>"),
  CONFIG_VALUE_STRING            ("cd-write-command",                 &globalOptions.cd.writeCommand,-1,                             "<command>"),
  CONFIG_VALUE_STRING            ("cd-write-image-command",           &globalOptions.cd.writeImageCommand,-1,                        "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_STRING            ("dvd-device",                       &globalOptions.dvd.deviceName,-1,                              "<path>"),
  CONFIG_VALUE_STRING            ("dvd-request-volume-command",       &globalOptions.dvd.requestVolumeCommand,-1,                    "<command>"),
  CONFIG_VALUE_STRING            ("dvd-unload-volume-command",        &globalOptions.dvd.unloadVolumeCommand,-1,                     "<command>"),
  CONFIG_VALUE_STRING            ("dvd-load-volume-command",          &globalOptions.dvd.loadVolumeCommand,-1,                       "<command>"),
  CONFIG_VALUE_INTEGER64         ("dvd-volume-size",                  &globalOptions.dvd.volumeSize,-1,                              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_VALUE_STRING            ("dvd-image-pre-command",            &globalOptions.dvd.imagePreProcessCommand,-1,                  "<command>"),
  CONFIG_VALUE_STRING            ("dvd-image-post-command",           &globalOptions.dvd.imagePostProcessCommand,-1,                 "<command>"),
  CONFIG_VALUE_STRING            ("dvd-image-command",                &globalOptions.dvd.imageCommand,-1,                            "<command>"),
  CONFIG_VALUE_STRING            ("dvd-ecc-pre-command",              &globalOptions.dvd.eccPreProcessCommand,-1,                    "<command>"),
  CONFIG_VALUE_STRING            ("dvd-ecc-post-command",             &globalOptions.dvd.eccPostProcessCommand,-1,                   "<command>"),
  CONFIG_VALUE_STRING            ("dvd-ecc-command",                  &globalOptions.dvd.eccCommand,-1,                              "<command>"),
  CONFIG_VALUE_STRING            ("dvd-blank-command",                &globalOptions.dvd.blankCommand,-1,                            "<command>"),
  CONFIG_VALUE_STRING            ("dvd-write-pre-command",            &globalOptions.dvd.writePreProcessCommand,-1,                  "<command>"),
  CONFIG_VALUE_STRING            ("dvd-write-post-command",           &globalOptions.dvd.writePostProcessCommand,-1,                 "<command>"),
  CONFIG_VALUE_STRING            ("dvd-write-command",                &globalOptions.dvd.writeCommand,-1,                            "<command>"),
  CONFIG_VALUE_STRING            ("dvd-write-image-command",          &globalOptions.dvd.writeImageCommand,-1,                       "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_STRING            ("bd-device",                        &globalOptions.bd.deviceName,-1,                               "<path>"),
  CONFIG_VALUE_STRING            ("bd-request-volume-command",        &globalOptions.bd.requestVolumeCommand,-1,                     "<command>"),
  CONFIG_VALUE_STRING            ("bd-unload-volume-command",         &globalOptions.bd.unloadVolumeCommand,-1,                      "<command>"),
  CONFIG_VALUE_STRING            ("bd-load-volume-command",           &globalOptions.bd.loadVolumeCommand,-1,                        "<command>"),
  CONFIG_VALUE_INTEGER64         ("bd-volume-size",                   &globalOptions.bd.volumeSize,-1,                               0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_VALUE_STRING            ("bd-image-pre-command",             &globalOptions.bd.imagePreProcessCommand,-1,                   "<command>"),
  CONFIG_VALUE_STRING            ("bd-image-post-command",            &globalOptions.bd.imagePostProcessCommand,-1,                  "<command>"),
  CONFIG_VALUE_STRING            ("bd-image-command",                 &globalOptions.bd.imageCommand,-1,                             "<command>"),
  CONFIG_VALUE_STRING            ("bd-ecc-pre-command",               &globalOptions.bd.eccPreProcessCommand,-1,                     "<command>"),
  CONFIG_VALUE_STRING            ("bd-ecc-post-command",              &globalOptions.bd.eccPostProcessCommand,-1,                    "<command>"),
  CONFIG_VALUE_STRING            ("bd-ecc-command",                   &globalOptions.bd.eccCommand,-1,                               "<command>"),
  CONFIG_VALUE_STRING            ("bd-blank-command",                 &globalOptions.bd.blankCommand,-1,                             "<command>"),
  CONFIG_VALUE_STRING            ("bd-write-pre-command",             &globalOptions.bd.writePreProcessCommand,-1,                   "<command>"),
  CONFIG_VALUE_STRING            ("bd-write-post-command",            &globalOptions.bd.writePostProcessCommand,-1,                  "<command>"),
  CONFIG_VALUE_STRING            ("bd-write-command",                 &globalOptions.bd.writeCommand,-1,                             "<command>"),
  CONFIG_VALUE_STRING            ("bd-write-image-command",           &globalOptions.bd.writeImageCommand,-1,                        "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_SEPARATOR(),
//  CONFIG_VALUE_INTEGER64         ("file-max-storage-size",            &defaultFileServer.maxStorageSize,-1,                        0LL,MAX_INT64,NULL,"<size>"),
  CONFIG_VALUE_BEGIN_SECTION     ("file-server",NULL,-1,NULL,NULL,NULL,NULL),
    CONFIG_STRUCT_VALUE_INTEGER64("file-max-storage-size",            Server,maxStorageSize,                                         0LL,MAX_INT64,NULL,"<size>"),
    CONFIG_STRUCT_VALUE_STRING   ("file-write-pre-command",           Server,writePreProcessCommand,                                 "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("file-write-post-command",          Server,writePostProcessCommand,                                "<command>"),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_SEPARATOR("ftp settings"),
  CONFIG_VALUE_STRING            ("ftp-login-name",                   &globalOptions.defaultFTPServer.ftp.loginName,-1,                            "<name>"),
  CONFIG_VALUE_SPECIAL           ("ftp-password",                     &globalOptions.defaultFTPServer.ftp.password,-1,                             configValuePasswordParse,configValuePasswordFormat,NULL),
  CONFIG_VALUE_INTEGER           ("ftp-max-connections",              &globalOptions.defaultFTPServer.maxConnectionCount,-1,                       0,MAX_INT,NULL,"<n>"),
  CONFIG_VALUE_INTEGER64         ("ftp-max-storage-size",             &globalOptions.defaultFTPServer.maxStorageSize,-1,                           0LL,MAX_INT64,NULL,"<size>"),
  CONFIG_VALUE_SPACE(),
  CONFIG_VALUE_BEGIN_SECTION     ("ftp-server",&globalOptions.serverList,-1,ConfigValue_listSectionDataIteratorInit,ConfigValue_listSectionDataIteratorDone,configValueServerFTPSectionDataIteratorNext,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-login-name",                   Server,ftp.loginName,                                          "<name>"),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ftp-password",                     Server,ftp.password,                                           configValuePasswordParse,configValuePasswordFormat,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("ftp-max-connections",              Server,maxConnectionCount,                                     0,MAX_INT,NULL,"<n>"),
    CONFIG_STRUCT_VALUE_INTEGER64("ftp-max-storage-size",             Server,maxStorageSize,                                         0LL,MAX_INT64,NULL,"<size>"),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-write-pre-command",            Server,writePreProcessCommand,                                 "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-write-post-command",           Server,writePostProcessCommand,                                "<command>"),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_SEPARATOR("ssh settings"),
  CONFIG_VALUE_INTEGER           ("ssh-port",                         &globalOptions.defaultSSHServer.ssh.port,-1,                                 0,65535,NULL,"<n>"),
  CONFIG_VALUE_STRING            ("ssh-login-name",                   &globalOptions.defaultSSHServer.ssh.loginName,-1,                            "<name>"),
  CONFIG_VALUE_SPECIAL           ("ssh-password",                     &globalOptions.defaultSSHServer.ssh.password,-1,                             configValuePasswordParse,configValuePasswordFormat,NULL),
  CONFIG_VALUE_SPECIAL           ("ssh-public-key",                   &globalOptions.defaultSSHServer.ssh.publicKey,-1,                            configValueKeyParse,configValueKeyFormat,NULL),
  CONFIG_VALUE_SPECIAL           ("ssh-private-key",                  &globalOptions.defaultSSHServer.ssh.privateKey,-1,                           configValueKeyParse,configValueKeyFormat,NULL),
  CONFIG_VALUE_INTEGER           ("ssh-max-connections",              &globalOptions.defaultSSHServer.maxConnectionCount,-1,                       0,MAX_INT,NULL,"<n>"),
  CONFIG_VALUE_INTEGER64         ("ssh-max-storage-size",             &globalOptions.defaultSSHServer.maxStorageSize,-1,                           0LL,MAX_INT64,NULL,"<size>"),
  CONFIG_VALUE_SPACE(),
  CONFIG_VALUE_BEGIN_SECTION     ("ssh-server",&globalOptions.serverList,-1,ConfigValue_listSectionDataIteratorInit,ConfigValue_listSectionDataIteratorDone,configValueServerSSHSectionDataIteratorNext,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("ssh-port",                         Server,ssh.port,                                               0,65535,NULL,"<n>"),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-login-name",                   Server,ssh.loginName,                                          "<name>"),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-password",                     Server,ssh.password,                                           configValuePasswordParse,configValuePasswordFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-public-key",                   Server,ssh.publicKey,                                          configValueKeyParse,configValueKeyFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-private-key",                  Server,ssh.privateKey,                                         configValueKeyParse,configValueKeyFormat,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("ssh-max-connections",              Server,maxConnectionCount,                                     0,MAX_INT,NULL,"<n>"),
    CONFIG_STRUCT_VALUE_INTEGER64("ssh-max-storage-size",             Server,maxStorageSize,                                         0LL,MAX_INT64,NULL,"<size>"),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-write-pre-command",            Server,writePreProcessCommand,                                 "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-write-post-command",           Server,writePostProcessCommand,                                "<command>"),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_SEPARATOR("webDAV settings"),
//  CONFIG_VALUE_INTEGER           ("webdav-port",                      &defaultWebDAVServer.webDAV.port,-1,                           0,65535,NULL,NULL,"<n>),
  CONFIG_VALUE_STRING            ("webdav-login-name",                &globalOptions.defaultWebDAVServer.webDAV.loginName,-1,                      "<name>"),
  CONFIG_VALUE_SPECIAL           ("webdav-password",                  &globalOptions.defaultWebDAVServer.webDAV.password,-1,                       configValuePasswordParse,configValuePasswordFormat,NULL),
  CONFIG_VALUE_INTEGER           ("webdav-max-connections",           &globalOptions.defaultWebDAVServer.maxConnectionCount,-1,                    0,MAX_INT,NULL,"<n>"),
  CONFIG_VALUE_INTEGER64         ("webdav-max-storage-size",          &globalOptions.defaultWebDAVServer.maxStorageSize,-1,                        0LL,MAX_INT64,NULL,"<size>"),
  CONFIG_VALUE_SPACE(),
  CONFIG_VALUE_BEGIN_SECTION     ("webdav-server",&globalOptions.serverList,-1,ConfigValue_listSectionDataIteratorInit,ConfigValue_listSectionDataIteratorDone,configValueServerWebDAVSectionDataIteratorNext,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-login-name",                Server,webDAV.loginName,                                       "<name>"),
    CONFIG_STRUCT_VALUE_SPECIAL  ("webdav-password",                  Server,webDAV.password,                                        configValuePasswordParse,configValuePasswordFormat,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("webdav-max-connections",           Server,maxConnectionCount,                                     0,MAX_INT,NULL,"<n>"),
    CONFIG_STRUCT_VALUE_INTEGER64("webdav-max-storage-size",          Server,maxStorageSize,                                         0LL,MAX_INT64,NULL,"<size>"),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-write-pre-command",         Server,writePreProcessCommand,                                 "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-write-post-command",        Server,writePostProcessCommand,                                "<command>"),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_SEPARATOR("device settings"),
  CONFIG_VALUE_STRING            ("device",                           &globalOptions.defaultDevice.name,-1,                                        "<name>"),
  CONFIG_VALUE_STRING            ("device-request-volume-command",    &globalOptions.defaultDevice.requestVolumeCommand,-1,                        "<command>"),
  CONFIG_VALUE_STRING            ("device-unload-volume-command",     &globalOptions.defaultDevice.unloadVolumeCommand,-1,                         "<command>"),
  CONFIG_VALUE_STRING            ("device-load-volume-command",       &globalOptions.defaultDevice.loadVolumeCommand,-1,                           "<command>"),
  CONFIG_VALUE_INTEGER64         ("device-volume-size",               &globalOptions.defaultDevice.volumeSize,-1,                                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_VALUE_STRING            ("device-image-pre-command",         &globalOptions.defaultDevice.imagePreProcessCommand,-1,                      "<command>"),
  CONFIG_VALUE_STRING            ("device-image-post-command",        &globalOptions.defaultDevice.imagePostProcessCommand,-1,                     "<command>"),
  CONFIG_VALUE_STRING            ("device-image-command",             &globalOptions.defaultDevice.imageCommand,-1,                                "<command>"),
  CONFIG_VALUE_STRING            ("device-ecc-pre-command",           &globalOptions.defaultDevice.eccPreProcessCommand,-1,                        "<command>"),
  CONFIG_VALUE_STRING            ("device-ecc-post-command",          &globalOptions.defaultDevice.eccPostProcessCommand,-1,                       "<command>"),
  CONFIG_VALUE_STRING            ("device-ecc-command",               &globalOptions.defaultDevice.eccCommand,-1,                                  "<command>"),
  CONFIG_VALUE_STRING            ("device-blank-command",             &globalOptions.defaultDevice.blankCommand,-1,                                "<command>"),
  CONFIG_VALUE_STRING            ("device-write-pre-command",         &globalOptions.defaultDevice.writePreProcessCommand,-1,                      "<command>"),
  CONFIG_VALUE_STRING            ("device-write-post-command",        &globalOptions.defaultDevice.writePostProcessCommand,-1,                     "<command>"),
  CONFIG_VALUE_STRING            ("device-write-command",             &globalOptions.defaultDevice.writeCommand,-1,                                "<command>"),
  CONFIG_VALUE_SPACE(),
  CONFIG_VALUE_SECTION_ARRAY("device",-1,
    CONFIG_STRUCT_VALUE_STRING   ("device-name",                      Device,name,                                                   "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-request-volume-command",    Device,requestVolumeCommand,                                   "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-unload-volume-command",     Device,unloadVolumeCommand,                                    "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-load-volume-command",       Device,loadVolumeCommand,                                      "<command>"),
    CONFIG_STRUCT_VALUE_INTEGER64("device-volume-size",               Device,volumeSize,                                             0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-pre-command",         Device,imagePreProcessCommand,                                 "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-post-command",        Device,imagePostProcessCommand,                                "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-command",             Device,imageCommand,                                           "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-pre-command",           Device,eccPreProcessCommand,                                   "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-post-command",          Device,eccPostProcessCommand,                                  "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-command",               Device,eccCommand,                                             "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-blank-command",             Device,blankCommand,                                           "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-pre-command",         Device,writePreProcessCommand,                                 "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-post-command",        Device,writePostProcessCommand,                                "<command>"),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-command",             Device,writeCommand,                                           "<command>"),
  ),

  // server settings
  CONFIG_VALUE_SEPARATOR("server"),
  CONFIG_VALUE_SELECT            ("server-mode",                      &globalOptions.serverMode,-1,                                  CONFIG_VALUE_SERVER_MODES,"<mode>"),
  CONFIG_VALUE_INTEGER           ("server-port",                      &globalOptions.serverPort,-1,                                  0,65535,NULL,"<n>"),
  CONFIG_VALUE_INTEGER           ("server-tls-port",                  &globalOptions.serverTLSPort,-1,                               0,65535,NULL,"<n>"),
//TODO: deprecated, use server-ca
  CONFIG_VALUE_SPECIAL           ("server-ca-file",                   &globalOptions.serverCA,-1,                                    configValueCertificateParse,configValueCertificateFormat,NULL),
//TODO: deprecated, use server-cert
  CONFIG_VALUE_SPECIAL           ("server-cert-file",                 &globalOptions.serverCert,-1,                                  configValueCertificateParse,configValueCertificateFormat,NULL),
//TODO: deprecated, use server-key
  CONFIG_VALUE_SPECIAL           ("server-key-file",                  &globalOptions.serverKey,-1,                                   configValueKeyParse,configValueKeyFormat,NULL),
  CONFIG_VALUE_SPECIAL           ("server-password",                  &globalOptions.serverPasswordHash,-1,                          configValueHashDataParse,configValueHashDataFormat,NULL),
  CONFIG_VALUE_INTEGER           ("server-max-connections",           &globalOptions.serverMaxConnections,-1,                        0,65535,NULL,"<n>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_SEPARATOR("log"),
  CONFIG_VALUE_SET               ("log",                              &globalOptions.logTypes,-1,                                    CONFIG_VALUE_LOG_TYPES,"<type,...>"),
  CONFIG_VALUE_CSTRING           ("log-file",                         &globalOptions.logFileName,-1,                                 "<file name>"),
  CONFIG_VALUE_CSTRING           ("log-format",                       &globalOptions.logFormat,-1,                                   "<file name>"),
  CONFIG_VALUE_CSTRING           ("log-post-command",                 &globalOptions.logPostCommand,-1,                              "<command>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_SEPARATOR("miscellaneous"),
  CONFIG_VALUE_BOOLEAN           ("skip-unreadable",                  &globalOptions.skipUnreadableFlag,-1,                          "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("raw-images",                       &globalOptions.rawImagesFlag,-1,                               "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("no-fragments-check",               &globalOptions.noFragmentsCheckFlag,-1,                        "yes|no"),
  CONFIG_VALUE_SELECT            ("archive-file-mode",                &globalOptions.archiveFileMode,-1,                             CONFIG_VALUE_ARCHIVE_FILE_MODES,"<mode>"),
  CONFIG_VALUE_SELECT            ("restore-entry-mode",               &globalOptions.restoreEntryMode,-1,                            CONFIG_VALUE_RESTORE_ENTRY_MODES,"<mode>"),
  CONFIG_VALUE_BOOLEAN           ("wait-first-volume",                &globalOptions.waitFirstVolumeFlag,-1,                         "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("no-signature",                     &globalOptions.noSignatureFlag,-1,                             "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("skip-verify-signatures",           &globalOptions.skipVerifySignaturesFlag,-1,                    "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("no-bar-on-medium",                 &globalOptions.noBAROnMediumFlag,-1,                           "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("no-stop-on-error",                 &globalOptions.noStopOnErrorFlag,-1,                           "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("no-stop-on-attribute-error",       &globalOptions.noStopOnAttributeErrorFlag,-1,                  "yes|no"),
  CONFIG_VALUE_BOOLEAN           ("quiet",                            &globalOptions.quietFlag,-1,                                   "yes|no"),
  CONFIG_VALUE_COMMENT           ("verbose level [0..6]"),
  CONFIG_VALUE_INTEGER           ("verbose",                          &globalOptions.verboseLevel,-1,                                0,6,NULL,"<n>"),

  CONFIG_VALUE_SPACE(),

  CONFIG_VALUE_COMMENT           ("BAR process id file"),
  CONFIG_VALUE_CSTRING           ("pid-file",                         &globalOptions.pidFileName,-1,                                 "<file name>"),
  CONFIG_VALUE_COMMENT("remote BAR executable"),
  CONFIG_VALUE_STRING            ("remote-bar-executable",            &globalOptions.remoteBARExecutable,-1,                         "<executable>"),

  CONFIG_VALUE_SPACE(),

  // deprecated
  CONFIG_VALUE_DEPRECATED        ("server-jobs-directory",            &globalOptions.jobsDirectory,-1,                               ConfigValue_parseDeprecatedString,NULL,"jobs-directory",TRUE),
  CONFIG_VALUE_DEPRECATED        ("remote-host-name",                 NULL,-1,                                                       NULL,NULL,"slave-host-name",TRUE),
  CONFIG_VALUE_DEPRECATED        ("remote-host-port",                 NULL,-1,                                                       NULL,NULL,"slave-host-port",TRUE),
  CONFIG_VALUE_DEPRECATED        ("remote-host-force-ssl",            NULL,-1,                                                       NULL,NULL,"slave-host-force-tls",TRUE),
  CONFIG_VALUE_DEPRECATED        ("slave-host-force-ssl",             NULL,-1,                                                       NULL,NULL,"slave-host-force-tls",TRUE),
  // Note: archive-file-mode=overwrite
  CONFIG_VALUE_DEPRECATED        ("overwrite-archive-files",          &globalOptions.archiveFileMode,-1,                             configValueDeprecatedArchiveFileModeOverwriteParse,NULL,"archive-file-mode",TRUE),
  // Note: restore-entry-mode=overwrite
  CONFIG_VALUE_DEPRECATED        ("overwrite-files",                  &globalOptions.restoreEntryMode,-1,                            configValueDeprecatedRestoreEntryModeOverwriteParse,NULL,"restore-entry-mode=overwrite",TRUE),
  CONFIG_VALUE_DEPRECATED        ("mount-device",                     &globalOptions.mountList,-1,                                   configValueDeprecatedMountDeviceParse,NULL,"mount",TRUE),
  CONFIG_VALUE_DEPRECATED        ("schedule",                         NULL,-1,                                                       NULL,NULL,NULL,TRUE),
  CONFIG_VALUE_DEPRECATED        ("stop-on-error",                    &globalOptions.noStopOnErrorFlag,-1,                           configValueDeprecatedStopOnErrorParse,NULL,"no-stop-on-error",TRUE),

  // ignored
);

void Configuration_initGlobalOptions(void)
{
  uint i;

  memClear(&globalOptions,sizeof(GlobalOptions));

  // --- program options
  globalOptions.runMode                                         = RUN_MODE_INTERACTIVE;
  globalOptions.barExecutable                                   = String_new();
  globalOptions.niceLevel                                       = 0;
  globalOptions.maxThreads                                      = 0;
  globalOptions.tmpDirectory                                    = String_newCString(DEFAULT_TMP_DIRECTORY);
  globalOptions.maxTmpSize                                      = 0LL;
  globalOptions.jobsDirectory                                   = String_newCString(DEFAULT_JOBS_DIRECTORY);
  globalOptions.incrementalDataDirectory                        = String_newCString(DEFAULT_INCREMENTAL_DATA_DIRECTORY);
  globalOptions.masterInfo.pairingFileName                      = DEFAULT_PAIRING_MASTER_FILE_NAME;
  globalOptions.masterInfo.name                                 = String_new();
  initHash(&globalOptions.masterInfo.uuidHash);
  Configuration_initKey(&globalOptions.masterInfo.publicKey);

  List_init(&globalOptions.maxBandWidthList);
  globalOptions.maxBandWidthList.n                              = 0L;
  globalOptions.maxBandWidthList.lastReadTimestamp              = 0LL;

  Semaphore_init(&globalOptions.serverList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&globalOptions.serverList);
  Semaphore_init(&globalOptions.deviceList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&globalOptions.deviceList);

  globalOptions.indexDatabaseUpdateFlag                         = TRUE;
  globalOptions.indexDatabaseAutoUpdateFlag                     = TRUE;
  List_init(&globalOptions.indexDatabaseMaxBandWidthList);
  globalOptions.indexDatabaseMaxBandWidthList.n                 = 0L;
  globalOptions.indexDatabaseMaxBandWidthList.lastReadTimestamp = 0LL;
  globalOptions.indexDatabaseKeepTime                           = S_PER_DAY;

  Semaphore_init(&globalOptions.maintenanceList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&globalOptions.maintenanceList);

  globalOptions.metaInfoFlag                                    = FALSE;
  globalOptions.groupFlag                                       = FALSE;
  globalOptions.allFlag                                         = FALSE;
  globalOptions.longFormatFlag                                  = FALSE;
  globalOptions.humanFormatFlag                                 = FALSE;
  globalOptions.numericUIDGIDFlag                               = FALSE;
  globalOptions.numericPermissionsFlag                          = FALSE;
  globalOptions.noHeaderFooterFlag                              = FALSE;
  globalOptions.deleteOldArchiveFilesFlag                       = FALSE;
  globalOptions.ignoreNoBackupFileFlag                          = FALSE;
  globalOptions.noDefaultConfigFlag                             = FALSE;
  globalOptions.forceDeltaCompressionFlag                       = FALSE;
  globalOptions.ignoreNoDumpAttributeFlag                       = FALSE;
  globalOptions.alwaysCreateImageFlag                           = FALSE;
  globalOptions.blankFlag                                       = FALSE;
  globalOptions.rawImagesFlag                                   = FALSE;
  globalOptions.noFragmentsCheckFlag                            = FALSE;
  globalOptions.noIndexDatabaseFlag                             = FALSE;
  globalOptions.forceVerifySignaturesFlag                       = FALSE;
  globalOptions.skipVerifySignaturesFlag                        = FALSE;
  globalOptions.noSignatureFlag                                 = FALSE;
  globalOptions.noBAROnMediumFlag                               = FALSE;
  globalOptions.noStopOnErrorFlag                               = FALSE;
  globalOptions.noStopOnAttributeErrorFlag                      = FALSE;

  globalOptions.jobUUIDOrName                                   = String_new();

  globalOptions.storageName                                     = String_new();
  EntryList_init(&globalOptions.includeEntryList);
  PatternList_init(&globalOptions.excludePatternList);

  globalOptions.changeToDirectory                               = NULL;

  globalOptions.command                                         = COMMAND_NONE;

  globalOptions.generateKeyBits                                 = MIN_ASYMMETRIC_CRYPT_KEY_BITS;
  globalOptions.generateKeyMode                                 = CRYPT_KEY_MODE_NONE;

  globalOptions.logTypes                                        = LOG_TYPE_NONE;
  globalOptions.logFileName                                     = NULL;
  globalOptions.logFormat                                       = DEFAULT_LOG_FORMAT;
  globalOptions.logPostCommand                                  = NULL;

  globalOptions.pidFileName                                     = NULL;

  globalOptions.quietFlag                                       = FALSE;
  globalOptions.verboseLevel                                    = DEFAULT_VERBOSE_LEVEL;
  globalOptions.daemonFlag                                      = FALSE;
  globalOptions.noDetachFlag                                    = FALSE;
  globalOptions.batchFlag                                       = FALSE;
  globalOptions.versionFlag                                     = FALSE;
  globalOptions.helpFlag                                        = FALSE;
  globalOptions.xhelpFlag                                       = FALSE;
  globalOptions.helpInternalFlag                                = FALSE;

  globalOptions.serverMode                                      = SERVER_MODE_MASTER;
  globalOptions.serverPort                                      = DEFAULT_SERVER_PORT;
  globalOptions.serverTLSPort                                   = DEFAULT_TLS_SERVER_PORT;
  initCertificate(&globalOptions.serverCA);
  initCertificate(&globalOptions.serverCert);
  Configuration_initKey(&globalOptions.serverKey);
  initHash(&globalOptions.serverPasswordHash);
  globalOptions.serverMaxConnections                            = DEFAULT_MAX_SERVER_CONNECTIONS;

  // --- job options default values
  globalOptions.archiveType                                     = ARCHIVE_TYPE_NORMAL;

  globalOptions.storageNameListStdin                            = FALSE;
  globalOptions.storageNameListFileName                         = String_new();
  globalOptions.storageNameCommand                              = String_new();

  globalOptions.includeFileListFileName                         = String_new();
  globalOptions.includeFileCommand                              = String_new();
  globalOptions.includeImageListFileName                        = String_new();
  globalOptions.includeImageCommand                             = String_new();
  globalOptions.excludeListFileName                             = String_new();
  globalOptions.excludeCommand                                  = String_new();

  List_init(&globalOptions.mountList);
  globalOptions.mountCommand                                    = String_newCString(MOUNT_COMMAND);
  globalOptions.mountDeviceCommand                              = String_newCString(MOUNT_DEVICE_COMMAND);
  globalOptions.unmountCommand                                  = String_newCString(UNMOUNT_COMMAND);

  PatternList_init(&globalOptions.compressExcludePatternList);
  DeltaSourceList_init(&globalOptions.deltaSourceList);

  globalOptions.archivePartSize                                 = 0LL;

  globalOptions.incrementalListFileName                         = String_new();

  globalOptions.directoryStripCount                             = 0;
  globalOptions.destination                                     = String_new();
  globalOptions.owner.userId                                    = FILE_DEFAULT_USER_ID;
  globalOptions.owner.groupId                                   = FILE_DEFAULT_GROUP_ID;
  globalOptions.permissions                                     = FILE_DEFAULT_PERMISSION;

  globalOptions.patternType                                     = PATTERN_TYPE_GLOB;

  globalOptions.fragmentSize                                    = DEFAULT_FRAGMENT_SIZE;
  globalOptions.maxStorageSize                                  = 0LL;
  globalOptions.volumeSize                                      = 0LL;

  globalOptions.compressMinFileSize                             = DEFAULT_COMPRESS_MIN_FILE_SIZE;
  globalOptions.continuousMaxSize                               = 0LL;
  globalOptions.continuousMinTimeDelta                          = 0LL;

  globalOptions.compressAlgorithms.delta                        = COMPRESS_ALGORITHM_NONE;
  globalOptions.compressAlgorithms.byte                         = COMPRESS_ALGORITHM_NONE;

  globalOptions.cryptType                                       = CRYPT_TYPE_SYMMETRIC;
  for (i = 0; i < 4; i++)
  {
    globalOptions.cryptAlgorithms[i] = CRYPT_ALGORITHM_NONE;
  }
  globalOptions.cryptPasswordMode                               = PASSWORD_MODE_DEFAULT;
  Password_init(&globalOptions.cryptPassword);
  Password_init(&globalOptions.cryptNewPassword);
  Configuration_initKey(&globalOptions.cryptPublicKey);
  Configuration_initKey(&globalOptions.cryptPrivateKey);
  Configuration_initKey(&globalOptions.signaturePublicKey);
  Configuration_initKey(&globalOptions.signaturePrivateKey);

  initServer(&globalOptions.defaultFileServer,NULL,SERVER_TYPE_FILE);
  initServer(&globalOptions.defaultFTPServer,NULL,SERVER_TYPE_FTP);
  initServer(&globalOptions.defaultSSHServer,NULL,SERVER_TYPE_SSH);
  initServer(&globalOptions.defaultWebDAVServer,NULL,SERVER_TYPE_WEBDAV);
  initDevice(&globalOptions.defaultDevice);
  globalOptions.fileServer                                      = &globalOptions.defaultFileServer;
  globalOptions.ftpServer                                       = &globalOptions.defaultFTPServer;
  globalOptions.sshServer                                       = &globalOptions.defaultSSHServer;
  globalOptions.webDAVServer                                    = &globalOptions.defaultWebDAVServer;
  globalOptions.device                                          = &globalOptions.defaultDevice;

  globalOptions.comment                                         = NULL;

  globalOptions.remoteBARExecutable                             = NULL;

  globalOptions.file.writePreProcessCommand                     = NULL;
  globalOptions.file.writePostProcessCommand                    = NULL;

  globalOptions.ftp.writePreProcessCommand                      = NULL;
  globalOptions.ftp.writePostProcessCommand                     = NULL;

  globalOptions.scp.writePreProcessCommand                      = NULL;
  globalOptions.scp.writePostProcessCommand                     = NULL;

  globalOptions.sftp.writePreProcessCommand                     = NULL;
  globalOptions.sftp.writePostProcessCommand                    = NULL;

  globalOptions.cd.deviceName                                   = String_newCString(DEFAULT_CD_DEVICE_NAME);
  globalOptions.cd.requestVolumeCommand                         = NULL;
  globalOptions.cd.unloadVolumeCommand                          = String_newCString(CD_UNLOAD_VOLUME_COMMAND);
  globalOptions.cd.loadVolumeCommand                            = String_newCString(CD_LOAD_VOLUME_COMMAND);
  globalOptions.cd.volumeSize                                   = DEFAULT_CD_VOLUME_SIZE;
  globalOptions.cd.imagePreProcessCommand                       = NULL;
  globalOptions.cd.imagePostProcessCommand                      = NULL;
  globalOptions.cd.imageCommand                                 = String_newCString(CD_IMAGE_COMMAND);
  globalOptions.cd.eccPreProcessCommand                         = NULL;
  globalOptions.cd.eccPostProcessCommand                        = NULL;
  globalOptions.cd.eccCommand                                   = String_newCString(CD_ECC_COMMAND);
  globalOptions.cd.blankCommand                                 = String_newCString(CD_BLANK_COMMAND);
  globalOptions.cd.writePreProcessCommand                       = NULL;
  globalOptions.cd.writePostProcessCommand                      = NULL;
  globalOptions.cd.writeCommand                                 = String_newCString(CD_WRITE_COMMAND);
  globalOptions.cd.writeImageCommand                            = String_newCString(CD_WRITE_IMAGE_COMMAND);

  globalOptions.dvd.deviceName                                  = String_newCString(DEFAULT_DVD_DEVICE_NAME);
  globalOptions.dvd.requestVolumeCommand                        = NULL;
  globalOptions.dvd.unloadVolumeCommand                         = String_newCString(DVD_UNLOAD_VOLUME_COMMAND);
  globalOptions.dvd.loadVolumeCommand                           = String_newCString(DVD_LOAD_VOLUME_COMMAND);
  globalOptions.dvd.volumeSize                                  = DEFAULT_DVD_VOLUME_SIZE;
  globalOptions.dvd.imagePreProcessCommand                      = NULL;
  globalOptions.dvd.imagePostProcessCommand                     = NULL;
  globalOptions.dvd.imageCommand                                = String_newCString(DVD_IMAGE_COMMAND);
  globalOptions.dvd.eccPreProcessCommand                        = NULL;
  globalOptions.dvd.eccPostProcessCommand                       = NULL;
  globalOptions.dvd.eccCommand                                  = String_newCString(DVD_ECC_COMMAND);
  globalOptions.dvd.blankCommand                                = String_newCString(DVD_BLANK_COMMAND);
  globalOptions.dvd.writePreProcessCommand                      = NULL;
  globalOptions.dvd.writePostProcessCommand                     = NULL;
  globalOptions.dvd.writeCommand                                = String_newCString(DVD_WRITE_COMMAND);
  globalOptions.dvd.writeImageCommand                           = String_newCString(DVD_WRITE_IMAGE_COMMAND);

  globalOptions.bd.deviceName                                   = String_newCString(DEFAULT_BD_DEVICE_NAME);
  globalOptions.bd.requestVolumeCommand                         = NULL;
  globalOptions.bd.unloadVolumeCommand                          = String_newCString(BD_UNLOAD_VOLUME_COMMAND);
  globalOptions.bd.loadVolumeCommand                            = String_newCString(BD_LOAD_VOLUME_COMMAND);
  globalOptions.bd.volumeSize                                   = DEFAULT_BD_VOLUME_SIZE;
  globalOptions.bd.imagePreProcessCommand                       = NULL;
  globalOptions.bd.imagePostProcessCommand                      = NULL;
  globalOptions.bd.imageCommand                                 = String_newCString(BD_IMAGE_COMMAND);
  globalOptions.bd.eccPreProcessCommand                         = NULL;
  globalOptions.bd.eccPostProcessCommand                        = NULL;
  globalOptions.bd.eccCommand                                   = String_newCString(BD_ECC_COMMAND);
  globalOptions.bd.blankCommand                                 = String_newCString(BD_BLANK_COMMAND);
  globalOptions.bd.writePreProcessCommand                       = NULL;
  globalOptions.bd.writePostProcessCommand                      = NULL;
  globalOptions.bd.writeCommand                                 = String_newCString(BD_WRITE_COMMAND);
  globalOptions.bd.writeImageCommand                            = String_newCString(BD_WRITE_IMAGE_COMMAND);

  globalOptions.archiveFileMode                                 = ARCHIVE_FILE_MODE_STOP;
  globalOptions.restoreEntryMode                                = RESTORE_ENTRY_MODE_STOP;
  globalOptions.skipUnreadableFlag                              = TRUE;
  globalOptions.errorCorrectionCodesFlag                        = FALSE;
  globalOptions.waitFirstVolumeFlag                             = FALSE;

  // debug/test only
  globalOptions.debug.serverLevel                                = DEFAULT_SERVER_DEBUG_LEVEL;
  globalOptions.debug.indexWaitOperationsFlag                    = FALSE;
  globalOptions.debug.indexPurgeDeletedStoragesFlag              = FALSE;
  globalOptions.debug.indexAddStorage                            = NULL;
  globalOptions.debug.indexRemoveStorage                         = NULL;
  globalOptions.debug.indexRefreshStorage                        = NULL;

  VALUESET_CLEAR(globalOptionSet);
}

void Configuration_doneGlobalOptions(void)
{
  // debug/test only
  String_delete(globalOptions.debug.indexRefreshStorage);
  String_delete(globalOptions.debug.indexRemoveStorage);
  String_delete(globalOptions.debug.indexAddStorage);

  // --- job options default values
  String_delete(globalOptions.bd.writeImageCommand);
  String_delete(globalOptions.bd.writeCommand);
  String_delete(globalOptions.bd.writePostProcessCommand);
  String_delete(globalOptions.bd.writePreProcessCommand);
  String_delete(globalOptions.bd.blankCommand);
  String_delete(globalOptions.bd.eccCommand);
  String_delete(globalOptions.bd.eccPostProcessCommand);
  String_delete(globalOptions.bd.eccPreProcessCommand);
  String_delete(globalOptions.bd.imageCommand);
  String_delete(globalOptions.bd.imagePostProcessCommand);
  String_delete(globalOptions.bd.imagePreProcessCommand);
  String_delete(globalOptions.bd.loadVolumeCommand);
  String_delete(globalOptions.bd.unloadVolumeCommand);
  String_delete(globalOptions.bd.requestVolumeCommand);
  String_delete(globalOptions.bd.deviceName);

  String_delete(globalOptions.dvd.writeImageCommand);
  String_delete(globalOptions.dvd.writeCommand);
  String_delete(globalOptions.dvd.writePostProcessCommand);
  String_delete(globalOptions.dvd.writePreProcessCommand);
  String_delete(globalOptions.dvd.blankCommand);
  String_delete(globalOptions.dvd.eccCommand);
  String_delete(globalOptions.dvd.eccPostProcessCommand);
  String_delete(globalOptions.dvd.eccPreProcessCommand);
  String_delete(globalOptions.dvd.imageCommand);
  String_delete(globalOptions.dvd.imagePostProcessCommand);
  String_delete(globalOptions.dvd.imagePreProcessCommand);
  String_delete(globalOptions.dvd.loadVolumeCommand);
  String_delete(globalOptions.dvd.unloadVolumeCommand);
  String_delete(globalOptions.dvd.requestVolumeCommand);
  String_delete(globalOptions.dvd.deviceName);

  String_delete(globalOptions.cd.writeImageCommand);
  String_delete(globalOptions.cd.writeCommand);
  String_delete(globalOptions.cd.writePostProcessCommand);
  String_delete(globalOptions.cd.writePreProcessCommand);
  String_delete(globalOptions.cd.blankCommand);
  String_delete(globalOptions.cd.eccCommand);
  String_delete(globalOptions.cd.eccPostProcessCommand);
  String_delete(globalOptions.cd.eccPreProcessCommand);
  String_delete(globalOptions.cd.imageCommand);
  String_delete(globalOptions.cd.imagePostProcessCommand);
  String_delete(globalOptions.cd.imagePreProcessCommand);
  String_delete(globalOptions.cd.loadVolumeCommand);
  String_delete(globalOptions.cd.unloadVolumeCommand);
  String_delete(globalOptions.cd.requestVolumeCommand);
  String_delete(globalOptions.cd.deviceName);

  String_delete(globalOptions.sftp.writePostProcessCommand);
  String_delete(globalOptions.sftp.writePreProcessCommand);

  String_delete(globalOptions.scp.writePostProcessCommand);
  String_delete(globalOptions.scp.writePreProcessCommand);

  String_delete(globalOptions.ftp.writePostProcessCommand);
  String_delete(globalOptions.ftp.writePreProcessCommand);

  String_delete(globalOptions.file.writePostProcessCommand);
  String_delete(globalOptions.file.writePreProcessCommand);

  String_delete(globalOptions.remoteBARExecutable);

  String_delete(globalOptions.comment);

  doneDevice(&globalOptions.defaultDevice);
  doneServer(&globalOptions.defaultWebDAVServer);
  doneServer(&globalOptions.defaultSSHServer);
  doneServer(&globalOptions.defaultFTPServer);
  doneServer(&globalOptions.defaultFileServer);

  List_done(&globalOptions.indexDatabaseMaxBandWidthList,CALLBACK_((ListNodeFreeFunction)freeBandWidthNode,NULL));

  Configuration_doneKey(&globalOptions.signaturePrivateKey);
  Configuration_doneKey(&globalOptions.signaturePublicKey);
  Configuration_doneKey(&globalOptions.cryptPrivateKey);
  Configuration_doneKey(&globalOptions.cryptPublicKey);
  Password_done(&globalOptions.cryptNewPassword);
  Password_done(&globalOptions.cryptPassword);

  String_delete(globalOptions.destination);

  String_delete(globalOptions.incrementalListFileName);

  DeltaSourceList_done(&globalOptions.deltaSourceList);
  PatternList_done(&globalOptions.compressExcludePatternList);

  String_delete(globalOptions.unmountCommand);
  String_delete(globalOptions.mountDeviceCommand);
  String_delete(globalOptions.mountCommand);
  List_done(&globalOptions.mountList,CALLBACK_((ListNodeFreeFunction)freeMountNode,NULL));

  String_delete(globalOptions.excludeCommand);
  String_delete(globalOptions.excludeListFileName);
  String_delete(globalOptions.includeImageCommand);
  String_delete(globalOptions.includeImageListFileName);
  String_delete(globalOptions.includeFileCommand);
  String_delete(globalOptions.includeFileListFileName);

  String_delete(globalOptions.storageNameCommand);
  String_delete(globalOptions.storageNameListFileName);

  // --- program options
  doneHash(&globalOptions.serverPasswordHash);
  Configuration_doneKey(&globalOptions.serverKey);
  doneCertificate(&globalOptions.serverCert);
  doneCertificate(&globalOptions.serverCA);

  PatternList_done(&globalOptions.excludePatternList);
  EntryList_done(&globalOptions.includeEntryList);
  String_delete(globalOptions.storageName);

  String_delete(globalOptions.jobUUIDOrName);

  List_done(&globalOptions.maintenanceList,CALLBACK_((ListNodeFreeFunction)freeMaintenanceNode,NULL));
  Semaphore_done(&globalOptions.maintenanceList.lock);

  List_done(&globalOptions.deviceList,CALLBACK_((ListNodeFreeFunction)freeDeviceNode,NULL));
  Semaphore_done(&globalOptions.deviceList.lock);
  List_done(&globalOptions.serverList,CALLBACK_((ListNodeFreeFunction)Configuration_freeServerNode,NULL));
  Semaphore_done(&globalOptions.serverList.lock);

  List_done(&globalOptions.maxBandWidthList,CALLBACK_((ListNodeFreeFunction)freeBandWidthNode,NULL));

  Configuration_doneKey(&globalOptions.masterInfo.publicKey);
  doneHash(&globalOptions.masterInfo.uuidHash);
  String_delete(globalOptions.masterInfo.name);
  String_delete(globalOptions.incrementalDataDirectory);
  String_delete(globalOptions.jobsDirectory);
  String_delete(globalOptions.tmpDirectory);
  String_delete(globalOptions.barExecutable);

  List_done(&configFileList,CALLBACK_((ListNodeFreeFunction)freeConfigFileNode,NULL));
}

MaintenanceNode *Configuration_newMaintenanceNode(void)
{
  MaintenanceNode *maintenanceNode;

  maintenanceNode = LIST_NEW_NODE(MaintenanceNode);
  if (maintenanceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  maintenanceNode->id               = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  maintenanceNode->date.year        = DATE_ANY;
  maintenanceNode->date.month       = DATE_ANY;
  maintenanceNode->date.day         = DATE_ANY;
  maintenanceNode->weekDaySet       = WEEKDAY_SET_ANY;
  maintenanceNode->beginTime.hour   = TIME_ANY;
  maintenanceNode->beginTime.minute = TIME_ANY;
  maintenanceNode->endTime.hour     = TIME_ANY;
  maintenanceNode->endTime.minute   = TIME_ANY;

  return maintenanceNode;
}

void Configuration_deleteMaintenanceNode(MaintenanceNode *maintenanceNode)
{
  assert(maintenanceNode != NULL);

  freeMaintenanceNode(maintenanceNode,NULL);
  LIST_DELETE_NODE(maintenanceNode);
}

ServerNode *Configuration_newServerNode(ConstString name, ServerTypes serverType)
{
  ServerNode *serverNode;

  assert(name != NULL);

  // allocate server node
  serverNode = LIST_NEW_NODE(ServerNode);
  if (serverNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  serverNode->id                                  = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  initServer(serverNode,name,serverType);
  serverNode->connection.lowPriorityRequestCount  = 0;
  serverNode->connection.highPriorityRequestCount = 0;
  serverNode->connection.count                    = 0;

  return serverNode;
}

void Configuration_freeServerNode(ServerNode *serverNode, void *userData)
{
  assert(serverNode != NULL);

  UNUSED_VARIABLE(userData);

  doneServer(serverNode);
}

void Configuration_deleteServerNode(ServerNode *serverNode)
{
  assert(serverNode != NULL);

  Configuration_freeServerNode(serverNode,NULL);
  LIST_DELETE_NODE(serverNode);
}

void Configuration_add(ConfigFileTypes configFileType, const char *fileName)
{
  ConfigFileNode *configFileNode;

  configFileNode = newConfigFileNode(configFileType,
                                     fileName
                                    );
  assert(configFileNode != NULL);
  List_append(&configFileList,configFileNode);
}

Errors Configuration_readAll(bool printInfoFlag)
{
  const ConfigFileNode *configFileNode;

  LIST_ITERATE(&configFileList,configFileNode)
  {
    if (!readConfigFile(configFileNode->fileName,printInfoFlag))
    {
      return ERROR_CONFIG;
    }
  }

  return ERROR_NONE;
}

Errors Configuration_update(void)
{
  String                configFileName;
  StringList            configLinesList;
  String                line;
  Errors                error;
  uint                  i;
  StringNode            *nextStringNode;
  ConfigValueFormat     configValueFormat;
  const MaintenanceNode *maintenanceNode;
  const ServerNode      *serverNode;

  // init variables
  configFileName = String_new();
  StringList_init(&configLinesList);
  line           = String_new();

  // get config file name
  if (String_isEmpty(getConfigFileName(configFileName)))
  {
    String_delete(line);
    StringList_done(&configLinesList);
    String_delete(configFileName);
    return ERROR_NO_WRITABLE_CONFIG;
  }

  // read config file lines
  error = ConfigValue_readConfigFileLines(configFileName,&configLinesList);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    StringList_done(&configLinesList);
    String_delete(configFileName);
    return error;
  }

  // update config entries
  CONFIG_VALUE_ITERATE(CONFIG_VALUES,NULL,i)
  {
    // delete old entries, get position for insert new entries
    nextStringNode = ConfigValue_deleteEntries(&configLinesList,NULL,CONFIG_VALUES[i].name);

    // insert new entries
    ConfigValue_formatInit(&configValueFormat,
                           &CONFIG_VALUES[i],
                           CONFIG_VALUE_FORMAT_MODE_LINE,
                           NULL  // variable
                          );
    while (ConfigValue_format(&configValueFormat,line))
    {
      StringList_insert(&configLinesList,line,nextStringNode);
    }
    ConfigValue_formatDone(&configValueFormat);
  }

  // update maintenance
  nextStringNode = ConfigValue_deleteSections(&configLinesList,"maintenance");
  SEMAPHORE_LOCKED_DO(&globalOptions.maintenanceList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.maintenanceList,maintenanceNode)
    {
      String_format(line,"[maintenance]");
      StringList_insert(&configLinesList,line,nextStringNode);

      CONFIG_VALUE_ITERATE(CONFIG_VALUES,"maintenance",i)
      {
        ConfigValue_formatInit(&configValueFormat,
                               &CONFIG_VALUES[i],
                               CONFIG_VALUE_FORMAT_MODE_LINE,
                               maintenanceNode
                              );
        while (ConfigValue_format(&configValueFormat,line))
        {
          StringList_insert(&configLinesList,line,nextStringNode);
        }
        ConfigValue_formatDone(&configValueFormat);
      }

      StringList_insertCString(&configLinesList,"[end]",nextStringNode);
      StringList_insertCString(&configLinesList,"",nextStringNode);
    }
  }

  // update storage servers
  nextStringNode = ConfigValue_deleteSections(&configLinesList,"file-server");
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->type == SERVER_TYPE_FILE)
      {
        // insert new file server section
        String_format(line,"[file-server %'S]",serverNode->name);
        StringList_insert(&configLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(CONFIG_VALUES,"file-server",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 serverNode
                                );
          while (ConfigValue_format(&configValueFormat,line))
          {
            StringList_insert(&configLinesList,line,nextStringNode);
          }
          ConfigValue_formatDone(&configValueFormat);
        }

        StringList_insertCString(&configLinesList,"[end]",nextStringNode);
        StringList_insertCString(&configLinesList,"",nextStringNode);
      }
    }
  }
  nextStringNode = ConfigValue_deleteSections(&configLinesList,"ftp-server");
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->type == SERVER_TYPE_FTP)
      {
        // insert new ftp server section
//TODO: format configuration
//        StringList_insertCString(&configLinesList,"",nextStringNode);
//        StringList_insertCString(&configLinesList,"# ----------------------------------------------------------------------",nextStringNode);
//        StringList_insertCString(&configLinesList,"# FTP login settings",nextStringNode);
        String_format(line,"[ftp-server %'S]",serverNode->name);
        StringList_insert(&configLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(CONFIG_VALUES,"ftp-server",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 serverNode
                                );
          while (ConfigValue_format(&configValueFormat,line))
          {
            StringList_insert(&configLinesList,line,nextStringNode);
          }
          ConfigValue_formatDone(&configValueFormat);
        }

        StringList_insertCString(&configLinesList,"[end]",nextStringNode);
        StringList_insertCString(&configLinesList,"",nextStringNode);
      }
    }
  }
  nextStringNode = ConfigValue_deleteSections(&configLinesList,"ssh-server");
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->type == SERVER_TYPE_SSH)
      {
        // insert new ssh-server section
//TODO: format configuration
//        StringList_insertCString(&configLinesList,"",nextStringNode);
//        StringList_insertCString(&configLinesList,"# ----------------------------------------------------------------------",nextStringNode);
//        StringList_insertCString(&configLinesList,"# SSH/SCP/SFTP login settings",nextStringNode);
        String_format(line,"[ssh-server %'S]",serverNode->name);
        StringList_insert(&configLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(CONFIG_VALUES,"ssh-server",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 serverNode
                                );
          while (ConfigValue_format(&configValueFormat,line))
          {
            StringList_insert(&configLinesList,line,nextStringNode);
          }
          ConfigValue_formatDone(&configValueFormat);
        }

        StringList_insertCString(&configLinesList,"[end]",nextStringNode);
        StringList_insertCString(&configLinesList,"",nextStringNode);
      }
    }
  }
  nextStringNode = ConfigValue_deleteSections(&configLinesList,"webdav-server");
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->type == SERVER_TYPE_WEBDAV)
      {
        // insert new webdav-server sections
//TODO: format configuration
//        StringList_insertCString(&configLinesList,"",nextStringNode);
//        StringList_insertCString(&configLinesList,"# ----------------------------------------------------------------------",nextStringNode);
//        StringList_insertCString(&configLinesList,"# WebDAV login settings",nextStringNode);
        String_format(line,"[webdav-server %'S]",serverNode->name);
        StringList_insert(&configLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(CONFIG_VALUES,"webdav-server",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 serverNode
                                );
          while (ConfigValue_format(&configValueFormat,line))
          {
            StringList_insert(&configLinesList,line,nextStringNode);
          }
          ConfigValue_formatDone(&configValueFormat);
        }

        StringList_insertCString(&configLinesList,"[end]",nextStringNode);
        StringList_insertCString(&configLinesList,"",nextStringNode);
      }
    }
  }

  // update master
  nextStringNode = ConfigValue_deleteSections(&configLinesList,"master");
  if (nextStringNode == NULL)
  {
//TODO: format configuration
//    StringList_insertCString(&configLinesList,"",nextStringNode);
//    StringList_insertCString(&configLinesList,"# ----------------------------------------------------------------------",nextStringNode);
//    StringList_insertCString(&configLinesList,"# master settings",nextStringNode);
  }
  String_format(line,"[master]");
  StringList_insert(&configLinesList,line,nextStringNode);
  CONFIG_VALUE_ITERATE(CONFIG_VALUES,"master",i)
  {
    ConfigValue_formatInit(&configValueFormat,
                           &CONFIG_VALUES[i],
                           CONFIG_VALUE_FORMAT_MODE_LINE,
                           &globalOptions.masterInfo
                          );
    while (ConfigValue_format(&configValueFormat,line))
    {
      StringList_insert(&configLinesList,line,nextStringNode);
    }
    ConfigValue_formatDone(&configValueFormat);
  }
  StringList_insertCString(&configLinesList,"[end]",nextStringNode);
  StringList_insertCString(&configLinesList,"",nextStringNode);

  // write config file lines
  error = ConfigValue_writeConfigFileLines(configFileName,&configLinesList);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    StringList_done(&configLinesList);
    String_delete(configFileName);
    return error;
  }
  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Updated configuration file '%s'",
             String_cString(configFileName)
            );

  // free resources
  String_delete(line);
  StringList_done(&configLinesList);
  String_delete(configFileName);

  return ERROR_NONE;
}

Errors Configuration_readCertificateFile(Certificate *certificate, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  uint64     dataLength;
  void       *data;

  assert(certificate != NULL);
  assert(fileName != NULL);

  error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get file size
  dataLength = File_getSize(&fileHandle);
  if (dataLength == 0LL)
  {
    (void)File_close(&fileHandle);
    return ERROR_NO_TLS_CERTIFICATE;
  }

  // allocate memory
  data = malloc((size_t)dataLength);
  if (data == NULL)
  {
    (void)File_close(&fileHandle);
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // read file data
  error = File_read(&fileHandle,
                    data,
                    dataLength,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    free(data);
    (void)File_close(&fileHandle);
    return error;
  }

  // close file
  (void)File_close(&fileHandle);

  // set certificate data
  if (certificate->data != NULL) free(certificate->data);
  certificate->data   = data;
  certificate->length = dataLength;

  printInfo(2,"Read certificate file '%s'\n",fileName);

  return ERROR_NONE;
}

Errors Configuration_readKeyFile(Key *key, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  uint       dataLength;
  char       *data;

  assert(key != NULL);
  assert(fileName != NULL);

  // open file
  error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get data size
  dataLength = File_getSize(&fileHandle);
  if (dataLength == 0LL)
  {
    (void)File_close(&fileHandle);
    return ERROR_NO_TLS_KEY;
  }

  // allocate secure memory
  data = (char*)allocSecure((size_t)dataLength);
  if (data == NULL)
  {
    (void)File_close(&fileHandle);
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // read file data
  error = File_read(&fileHandle,
                    data,
                    dataLength,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    freeSecure(data);
    (void)File_close(&fileHandle);
    return error;
  }

  // close file
  (void)File_close(&fileHandle);

  // set key data
  if (key->data != NULL) freeSecure(key->data);
  key->data   = data;
  key->length = dataLength;

  printInfo(3,"Read key file '%s'\n",fileName);

  return ERROR_NONE;
}

Errors Configuration_readAllServerKeys(void)
{
  String fileName;
  Key    key;

  // init default servers
  fileName = String_new();
  Configuration_initKey(&key);
  File_appendFileNameCString(String_setCString(fileName,getenv("HOME")),".ssh/id_rsa.pub");
  if (File_exists(fileName) && (Configuration_readKeyFile(&key,String_cString(fileName)) == ERROR_NONE))
  {
    Configuration_duplicateKey(&globalOptions.defaultSSHServer.ssh.publicKey,&key);
    Configuration_duplicateKey(&globalOptions.defaultWebDAVServer.webDAV.publicKey,&key);
  }
  File_appendFileNameCString(String_setCString(fileName,getenv("HOME")),".ssh/id_rsa");
  if (File_exists(fileName) && (Configuration_readKeyFile(&key,String_cString(fileName)) == ERROR_NONE))
  {
    Configuration_duplicateKey(&globalOptions.defaultSSHServer.ssh.privateKey,&key);
    Configuration_duplicateKey(&globalOptions.defaultWebDAVServer.webDAV.privateKey,&key);
  }
  Configuration_doneKey(&key);
  String_delete(fileName);

  // read default server CA, certificate, key
  (void)Configuration_readCertificateFile(&globalOptions.serverCA,DEFAULT_TLS_SERVER_CA_FILE);
  (void)Configuration_readCertificateFile(&globalOptions.serverCert,DEFAULT_TLS_SERVER_CERTIFICATE_FILE);
  (void)Configuration_readKeyFile(&globalOptions.serverKey,DEFAULT_TLS_SERVER_KEY_FILE);

  return ERROR_NONE;
}

bool Configuration_validate(void)
{
  if (!String_isEmpty(globalOptions.tmpDirectory))
  {
    if (!File_exists(globalOptions.tmpDirectory)) { printError(_("Temporary directory '%s' does not exists!"),String_cString(globalOptions.tmpDirectory)); return FALSE; }
    if (!File_isDirectory(globalOptions.tmpDirectory)) { printError(_("'%s' is not a directory!"),String_cString(globalOptions.tmpDirectory)); return FALSE; }
    if (!File_isWritable(globalOptions.tmpDirectory)) { printError(_("Temporary directory '%s' is not writable!"),String_cString(globalOptions.tmpDirectory)); return FALSE; }
  }

  if (!Continuous_isAvailable())
  {
    printWarning("Continuous support is not available");
  }

  return TRUE;
}

// ----------------------------------------------------------------------

ConfigValue JOB_CONFIG_VALUES[] = CONFIG_VALUE_ARRAY
(
  CONFIG_STRUCT_VALUE_STRING      ("UUID",                      JobNode,job.uuid                                 ,"<uuid>"),
  CONFIG_STRUCT_VALUE_STRING      ("slave-host-name",           JobNode,job.slaveHost.name                       ,"<name>"),
  CONFIG_STRUCT_VALUE_INTEGER     ("slave-host-port",           JobNode,job.slaveHost.port,                      0,65535,NULL,"<n>"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("slave-host-force-tls",      JobNode,job.slaveHost.forceTLS,                  "yes|no"),
  CONFIG_STRUCT_VALUE_STRING      ("archive-name",              JobNode,job.storageName                          ,"<name>"),
  CONFIG_STRUCT_VALUE_SELECT      ("archive-type",              JobNode,job.options.archiveType,                 CONFIG_VALUE_ARCHIVE_TYPES,"<type>"),

  CONFIG_STRUCT_VALUE_STRING      ("incremental-list-file",     JobNode,job.options.incrementalListFileName      ,"<file name>"),

  CONFIG_STRUCT_VALUE_INTEGER64   ("archive-part-size",         JobNode,job.options.archivePartSize,             0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),

  CONFIG_STRUCT_VALUE_INTEGER     ("directory-strip",           JobNode,job.options.directoryStripCount,         -1,MAX_INT,NULL,"<n>"),
  CONFIG_STRUCT_VALUE_STRING      ("destination",               JobNode,job.options.destination                  ,"<directory>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("owner",                     JobNode,job.options.owner,                       configValueOwnerParse,configValueOwnerFormat,NULL),

  CONFIG_STRUCT_VALUE_SELECT      ("pattern-type",              JobNode,job.options.patternType,                 CONFIG_VALUE_PATTERN_TYPES,"<type>"),

  CONFIG_STRUCT_VALUE_SPECIAL     ("compress-algorithm",        JobNode,job.options.compressAlgorithms,          configValueCompressAlgorithmsParse,configValueCompressAlgorithmsFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("compress-exclude",          JobNode,job.options.compressExcludePatternList,  configValuePatternParse,configValuePatternFormat,NULL),

  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-algorithm",           JobNode,job.options.cryptAlgorithms,             configValueCryptAlgorithmsParse,configValueCryptAlgorithmsFormat,NULL),
  CONFIG_STRUCT_VALUE_SELECT      ("crypt-type",                JobNode,job.options.cryptType,                   CONFIG_VALUE_CRYPT_TYPES,"<type>"),
  CONFIG_STRUCT_VALUE_SELECT      ("crypt-password-mode",       JobNode,job.options.cryptPasswordMode,           CONFIG_VALUE_PASSWORD_MODES,"<mode>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-password",            JobNode,job.options.cryptPassword,               configValuePasswordParse,configValuePasswordFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-public-key",          JobNode,job.options.cryptPublicKey,              configValueKeyParse,configValueKeyFormat,NULL),

  CONFIG_STRUCT_VALUE_STRING      ("pre-command",               JobNode,job.options.preProcessScript             ,"<command>"),
  CONFIG_STRUCT_VALUE_STRING      ("post-command",              JobNode,job.options.postProcessScript            ,"<command>"),
  CONFIG_STRUCT_VALUE_STRING      ("slave-pre-command",         JobNode,job.options.slavePreProcessScript        ,"<command>"),
  CONFIG_STRUCT_VALUE_STRING      ("slave-post-command",        JobNode,job.options.slavePostProcessScript       ,"<command>"),

  CONFIG_STRUCT_VALUE_BOOLEAN     ("storage-on-master",         JobNode,job.options.storageOnMaster,             "yes|no"),

  CONFIG_STRUCT_VALUE_STRING      ("ftp-login-name",            JobNode,job.options.ftpServer.loginName          ,"<name>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ftp-password",              JobNode,job.options.ftpServer.password,          configValuePasswordParse,configValuePasswordFormat,NULL),

  CONFIG_STRUCT_VALUE_INTEGER     ("ssh-port",                  JobNode,job.options.sshServer.port,              0,65535,NULL,"<n>"),
  CONFIG_STRUCT_VALUE_STRING      ("ssh-login-name",            JobNode,job.options.sshServer.loginName          ,"<name>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-password",              JobNode,job.options.sshServer.password,          configValuePasswordParse,configValuePasswordFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-public-key",            JobNode,job.options.sshServer.publicKey,         configValueKeyParse,configValueKeyFormat,NULL),
//  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-public-key-data",       JobNode,job.options.sshServer.publicKey,         configValueKeyParse,configValueKeyFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-private-key",           JobNode,job.options.sshServer.privateKey,        configValueKeyParse,configValueKeyFormat,NULL),
//  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-private-key-data",      JobNode,job.options.sshServer.privateKey,        configValueKeyParse,configValueKeyFormat,NULL),

  CONFIG_STRUCT_VALUE_SPECIAL     ("include-file",              JobNode,job.includeEntryList,                    configValueFileEntryPatternParse,configValueFileEntryPatternFormat,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("include-file-list",         JobNode,job.options.includeFileListFileName      ,"<file name>"),
  CONFIG_STRUCT_VALUE_STRING      ("include-file-command",      JobNode,job.options.includeFileCommand           ,"<command>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("include-image",             JobNode,job.includeEntryList,                    configValueImageEntryPatternParse,configValueImageEntryPatternFormat,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("include-image-list",        JobNode,job.options.includeImageListFileName     ,"<file name>"),
  CONFIG_STRUCT_VALUE_STRING      ("include-image-command",     JobNode,job.options.includeImageCommand          ,"<command>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("exclude",                   JobNode,job.excludePatternList,                  configValuePatternParse,configValuePatternFormat,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("exclude-list",              JobNode,job.options.excludeListFileName          ,"<file name>"),
  CONFIG_STRUCT_VALUE_STRING      ("exclude-command",           JobNode,job.options.excludeCommand               ,"<command>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("delta-source",              JobNode,job.options.deltaSourceList,             configValueDeltaSourceParse,configValueDeltaSourceFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("mount",                     JobNode,job.options.mountList,                   configValueMountParse,configValueMountFormat,NULL),

  CONFIG_STRUCT_VALUE_INTEGER64   ("max-storage-size",          JobNode,job.options.maxStorageSize,              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_STRUCT_VALUE_INTEGER64   ("volume-size",               JobNode,job.options.volumeSize,                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("ecc",                       JobNode,job.options.errorCorrectionCodesFlag,    "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("blank",                     JobNode,job.options.blankFlag,                   "yes|no"),

  CONFIG_STRUCT_VALUE_BOOLEAN     ("skip-unreadable",           JobNode,job.options.skipUnreadableFlag,          "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("raw-images",                JobNode,job.options.rawImagesFlag,               "yes|no"),
  CONFIG_STRUCT_VALUE_SELECT      ("archive-file-mode",         JobNode,job.options.archiveFileMode,             CONFIG_VALUE_ARCHIVE_FILE_MODES,"<mode>"),
  CONFIG_STRUCT_VALUE_SELECT      ("restore-entry-mode",        JobNode,job.options.restoreEntryMode,            CONFIG_VALUE_RESTORE_ENTRY_MODES,"<mode>"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("wait-first-volume",         JobNode,job.options.waitFirstVolumeFlag,         "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-signature",              JobNode,job.options.noSignatureFlag,             "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-bar-on-medium",          JobNode,job.options.noBAROnMediumFlag,           "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-stop-on-error",          JobNode,job.options.noStopOnErrorFlag,           "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-stop-on-attribute-error",JobNode,job.options.noStopOnAttributeErrorFlag,  "yes|no"),

  CONFIG_VALUE_BEGIN_SECTION("schedule",NULL,-1,NULL,NULL,NULL,NULL),
    CONFIG_STRUCT_VALUE_STRING    ("UUID",                      ScheduleNode,uuid                                ,"<uuid>"),
    CONFIG_STRUCT_VALUE_STRING    ("parentUUID",                ScheduleNode,parentUUID                          ,"<uuid>"),
    CONFIG_STRUCT_VALUE_SPECIAL   ("date",                      ScheduleNode,date,                               configValueScheduleDateParse,configValueScheduleDateFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("weekdays",                  ScheduleNode,weekDaySet,                         configValueScheduleWeekDaySetParse,configValueScheduleWeekDaySetFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("time",                      ScheduleNode,time,                               configValueScheduleTimeParse,configValueScheduleTimeFormat,NULL),
    CONFIG_STRUCT_VALUE_SELECT    ("archive-type",              ScheduleNode,archiveType,                        CONFIG_VALUE_ARCHIVE_TYPES,"<type>"),
    CONFIG_STRUCT_VALUE_INTEGER   ("interval",                  ScheduleNode,interval,                           0,MAX_INT,NULL,"<n>"),
    CONFIG_STRUCT_VALUE_STRING    ("text",                      ScheduleNode,customText                          ,"<text>"),
    CONFIG_STRUCT_VALUE_BOOLEAN   ("no-storage",                ScheduleNode,noStorage,                          "yes|no"),
    CONFIG_STRUCT_VALUE_BOOLEAN   ("enabled",                   ScheduleNode,enabled,                            "yes|no"),

    // deprecated
    CONFIG_VALUE_DEPRECATED       ("min-keep",                  NULL,-1,                                         configValueDeprecatedScheduleMinKeepParse,NULL,NULL,FALSE),
    CONFIG_VALUE_DEPRECATED       ("max-keep",                  NULL,-1,                                         configValueDeprecatedScheduleMaxKeepParse,NULL,NULL,FALSE),
    CONFIG_VALUE_DEPRECATED       ("max-age",                   NULL,-1,                                         configValueDeprecatedScheduleMaxAgeParse,NULL,NULL,FALSE),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_BEGIN_SECTION("persistence",NULL,-1,NULL,NULL,NULL,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("min-keep",                  PersistenceNode,minKeep,                         configValuePersistenceMinKeepParse,configValuePersistenceMinKeepFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("max-keep",                  PersistenceNode,maxKeep,                         configValuePersistenceMaxKeepParse,configValuePersistenceMaxKeepFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("max-age",                   PersistenceNode,maxAge,                          configValuePersistenceMaxAgeParse,configValuePersistenceMaxAgeFormat,NULL),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_STRUCT_VALUE_STRING      ("comment",                   JobNode,job.options.comment                      ,"<text>"),

  // deprecated
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-name",          JobNode,job.slaveHost.name,                      configValueDeprecatedRemoteHostParse,NULL,"slave-host-name",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-port",          JobNode,job.slaveHost.port,                      configValueDeprecatedRemotePortParse,NULL,"slave-host-port",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-force-ssl",     JobNode,job.slaveHost.forceTLS,                  ConfigValue_parseDeprecatedBoolean,NULL,"slave-host-force-tls",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("slave-host-force-ssl",      JobNode,job.slaveHost.forceTLS,                  ConfigValue_parseDeprecatedBoolean,NULL,"slave-host-force-tls",TRUE),
  // Note: archive-file-mode=overwrite
  CONFIG_STRUCT_VALUE_DEPRECATED  ("overwrite-archive-files",   JobNode,job.options.archiveFileMode,             configValueDeprecatedArchiveFileModeOverwriteParse,NULL,"archive-file-mode",TRUE),
  // Note: restore-entry-mode=overwrite
  CONFIG_STRUCT_VALUE_DEPRECATED  ("overwrite-files",           JobNode,job.options.restoreEntryMode,            configValueDeprecatedRestoreEntryModeOverwriteParse,NULL,"restore-entry-mode=overwrite",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("mount-device",              JobNode,job.options.mountList,                   configValueDeprecatedMountDeviceParse,NULL,"mount",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("stop-on-error",             JobNode,job.options.noStopOnErrorFlag,           configValueDeprecatedStopOnErrorParse,NULL,"no-stop-on-error",TRUE),

  // ignored
  CONFIG_VALUE_IGNORE             ("schedule",                                                                   NULL,TRUE),
);

#ifdef __cplusplus
  }
#endif

/* end of file */