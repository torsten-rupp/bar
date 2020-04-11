/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver main program
* Systems: all
*
\***********************************************************************/

#define __BAR_IMPLEMENTATION__

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

#include "bar_global.h"
#include "commands_create.h"
#include "commands_list.h"
#include "commands_restore.h"
#include "commands_test.h"
#include "commands_compare.h"
#include "commands_convert.h"
#include "jobs.h"
#include "server.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define __VERSION_TO_STRING(z) __VERSION_TO_STRING_TMP(z)
#define __VERSION_TO_STRING_TMP(z) #z
#define VERSION_MAJOR_STRING __VERSION_TO_STRING(VERSION_MAJOR)
#define VERSION_MINOR_STRING __VERSION_TO_STRING(VERSION_MINOR)
#define VERSION_REPOSITORY_STRING __VERSION_TO_STRING(VERSION_REPOSITORY)
#define VERSION_STRING VERSION_MAJOR_STRING "." VERSION_MINOR_STRING " (rev. " VERSION_REPOSITORY_STRING ")"

#define MOUNT_TIMEOUT (1L*60L*MS_PER_SECOND)  // mount timeout [ms]

/***************************** Datatypes *******************************/

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
GlobalOptions            globalOptions;
GlobalOptionSet          globalOptionSet;
String                   uuid;
String                   tmpDirectory;
Semaphore                consoleLock;
#ifdef HAVE_NEWLOCALE
  locale_t               POSIXLocale;
#endif /* HAVE_NEWLOCALE */

// Note: initialized once only here
LOCAL bool               daemonFlag       = FALSE;
LOCAL bool               noDetachFlag     = FALSE;
LOCAL bool               versionFlag      = FALSE;
LOCAL bool               helpFlag         = FALSE;
LOCAL bool               xhelpFlag        = FALSE;
LOCAL bool               helpInternalFlag = FALSE;

LOCAL MountedList        mountedList;                      // list of mounts

LOCAL Commands           command;
LOCAL String             jobUUIDName;

LOCAL String             jobUUID;                          // UUID of job to execute
LOCAL String             storageName;
LOCAL EntryList          includeEntryList;                 // included entries
LOCAL PatternList        excludePatternList;               // excluded entry patterns

LOCAL StorageFlags       storageFlags;

LOCAL const char         *changeToDirectory;

LOCAL Server             defaultFileServer;
LOCAL Server             defaultFTPServer;
LOCAL Server             defaultSSHServer;
LOCAL Server             defaultWebDAVServer;
LOCAL Device             defaultDevice;
LOCAL ServerModes        serverMode;
LOCAL uint               serverPort;
LOCAL uint               serverTLSPort;
LOCAL Certificate        serverCA;
LOCAL Certificate        serverCert;
LOCAL Key                serverKey;
LOCAL Hash               serverPasswordHash;
LOCAL uint               serverMaxConnections;

LOCAL const char         *continuousDatabaseFileName;
LOCAL const char         *indexDatabaseFileName;

LOCAL ulong              logTypes;
LOCAL const char         *logFileName;
LOCAL const char         *logFormat;
LOCAL const char         *logPostCommand;

LOCAL bool               batchFlag;

LOCAL const char         *pidFileName;

LOCAL uint               generateKeyBits;
LOCAL uint               generateKeyMode;

/*---------------------------------------------------------------------*/

LOCAL StringList         configFileNameList;  // list of configuration files to read
LOCAL bool               configModified;

LOCAL Semaphore          logLock;
LOCAL FILE               *logFile = NULL;     // log file handle

LOCAL ThreadLocalStorage outputLineHandle;
LOCAL String             lastOutputLine;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

LOCAL void deletePIDFile(void);
LOCAL void doneAll(void);

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL bool cmdOptionParseString(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseConfigFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseEntryPattern(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePattern(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseMount(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseDeltaSource(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseCompressAlgorithms(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseCryptAlgorithms(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseBandWidth(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseOwner(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePermissions(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseHashData(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionReadCertificateFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionReadKeyFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseKeyData(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePublicPrivateKey(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseArchiveFileModeOverwrite(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseRestoreEntryModeOverwrite(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseStorageFlagDryRun(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseStorageFlagNoStorage(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);

// deprecated
LOCAL bool cmdOptionParseDeprecatedMountDevice(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseDeprecatedStopOnError(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);

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

LOCAL CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM         ("create",                            'c',0,1,command,                                         0,COMMAND_CREATE_FILES,                                        "create new files archives"                                                ),
  CMD_OPTION_ENUM         ("image",                             'm',0,1,command,                                         0,COMMAND_CREATE_IMAGES,                                       "create new images archives"                                               ),
  CMD_OPTION_ENUM         ("list",                              'l',0,1,command,                                         0,COMMAND_LIST,                                                "list contents of archives"                                                ),
  CMD_OPTION_ENUM         ("test",                              't',0,1,command,                                         0,COMMAND_TEST,                                                "test contents of archives"                                                ),
  CMD_OPTION_ENUM         ("compare",                           'd',0,1,command,                                         0,COMMAND_COMPARE,                                             "compare contents of archive swith files and images"                       ),
  CMD_OPTION_ENUM         ("extract",                           'x',0,1,command,                                         0,COMMAND_RESTORE,                                             "restore archives"                                                         ),
  CMD_OPTION_ENUM         ("convert",                           0,  0,1,command,                                         0,COMMAND_CONVERT,                                             "convert archives"                                                         ),
  CMD_OPTION_ENUM         ("generate-keys",                     0,  0,1,command,                                         0,COMMAND_GENERATE_ENCRYPTION_KEYS,                            "generate new public/private key pair for encryption"                      ),
  CMD_OPTION_ENUM         ("generate-signature-keys",           0,  0,1,command,                                         0,COMMAND_GENERATE_SIGNATURE_KEYS,                             "generate new public/private key pair for signature"                       ),
//  CMD_OPTION_ENUM         ("new-key-password",                  0,  1,1,command,                                         0,COMMAND_NEW_KEY_PASSWORD,                                    "set new private key password"                                             ),
  CMD_OPTION_INTEGER      ("generate-keys-bits",                0,  1,1,generateKeyBits,                                 0,MIN_ASYMMETRIC_CRYPT_KEY_BITS,
                                                                                                                           MAX_ASYMMETRIC_CRYPT_KEY_BITS,COMMAND_LINE_BITS_UNITS,       "key bits",NULL                                                            ),
  CMD_OPTION_SELECT       ("generate-keys-mode",                0,  1,2,generateKeyMode,                                 0,COMMAND_LINE_OPTIONS_GENERATE_KEY_MODES,                     "select generate key mode mode"                                            ),
  CMD_OPTION_STRING       ("job",                               0,  0,1,jobUUIDName,                                     0,                                                             "execute job","name or UUID"                                               ),

  CMD_OPTION_ENUM         ("normal",                            0,  1,2,globalOptions.archiveType,                       0,ARCHIVE_TYPE_NORMAL,                                         "create normal archive (no incremental list file, default)"                ),
  CMD_OPTION_ENUM         ("full",                              'f',0,2,globalOptions.archiveType,                       0,ARCHIVE_TYPE_FULL,                                           "create full archive and incremental list file"                            ),
  CMD_OPTION_ENUM         ("incremental",                       'i',0,2,globalOptions.archiveType,                       0,ARCHIVE_TYPE_INCREMENTAL,                                    "create incremental archive"                                               ),
  CMD_OPTION_SPECIAL      ("incremental-list-file",             'I',1,2,&globalOptions.incrementalListFileName,          0,cmdOptionParseString,NULL,1,                                 "incremental list file name (default: <archive name>.bid)","file name"     ),
  CMD_OPTION_ENUM         ("differential",                      0,  1,2,globalOptions.archiveType,                       0,ARCHIVE_TYPE_DIFFERENTIAL,                                   "create differential archive"                                              ),

  CMD_OPTION_SELECT       ("pattern-type",                      0,  1,2,globalOptions.patternType,                       0,COMMAND_LINE_OPTIONS_PATTERN_TYPES,                          "select pattern type"                                                      ),

  CMD_OPTION_BOOLEAN      ("storage-list-stdin",                'T',1,3,globalOptions.storageNameListStdin,              0,                                                             "read storage name list from stdin"                                        ),
  CMD_OPTION_STRING       ("storage-list",                      0,  1,3,globalOptions.storageNameListFileName,           0,                                                             "storage name list file name","file name"                                  ),
  CMD_OPTION_STRING       ("storage-command",                   0,  1,3,globalOptions.storageNameCommand,                0,                                                             "storage name command","command"                                           ),

  CMD_OPTION_SPECIAL      ("include",                           '#',0,3,&includeEntryList,                               0,cmdOptionParseEntryPattern,NULL,1,                           "include pattern","pattern"                                                ),
  CMD_OPTION_STRING       ("include-file-list",                 0,  1,3,globalOptions.includeFileListFileName,           0,                                                             "include file pattern list file name","file name"                          ),
  CMD_OPTION_STRING       ("include-file-command",              0,  1,3,globalOptions.includeFileCommand,                0,                                                             "include file pattern command","command"                                   ),
  CMD_OPTION_STRING       ("include-image-list",                0,  1,3,globalOptions.includeImageListFileName,          0,                                                             "include image pattern list file name","file name"                         ),
  CMD_OPTION_STRING       ("include-image-command",             0,  1,3,globalOptions.includeImageCommand,               0,                                                             "include image pattern command","command"                                  ),
  CMD_OPTION_SPECIAL      ("exclude",                           '!',0,3,&excludePatternList,                             0,cmdOptionParsePattern,NULL,1,                                "exclude pattern","pattern"                                                ),
  CMD_OPTION_STRING       ("exclude-list",                      0,  1,3,globalOptions.excludeListFileName,               0,                                                             "exclude pattern list file name","file name"                               ),
  CMD_OPTION_STRING       ("exclude-command",                   0,  1,3,globalOptions.excludeCommand,                    0,                                                             "exclude pattern command","command"                                        ),

  CMD_OPTION_SPECIAL      ("mount",                             0  ,1,3,&globalOptions.mountList,                        0,cmdOptionParseMount,NULL,1,                                  "mount device","name[,[device][,yes]"                                      ),
  CMD_OPTION_STRING       ("mount-command",                     0,  1,3,globalOptions.mountCommand,                      0,                                                             "mount command","command"                                                  ),
  CMD_OPTION_STRING       ("mount-device-command",              0,  1,3,globalOptions.mountDeviceCommand,                0,                                                             "mount device command","command"                                           ),
  CMD_OPTION_STRING       ("unmount-command",                   0,  1,3,globalOptions.unmountCommand,                    0,                                                             "unmount command","command"                                                ),

  CMD_OPTION_SPECIAL      ("delta-source",                      0,  0,3,&globalOptions.deltaSourceList,                  0,cmdOptionParseDeltaSource,NULL,1,                            "source pattern","pattern"                                                 ),

  CMD_OPTION_SPECIAL      ("config",                            0,  1,2,NULL,                                            0,cmdOptionParseConfigFile,NULL,1,                             "configuration file","file name"                                           ),

  CMD_OPTION_STRING       ("tmp-directory",                     0,  1,1,globalOptions.tmpDirectory,                      0,                                                             "temporary directory","path"                                               ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",                      0,  1,1,globalOptions.maxTmpSize,                        0,0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "max. size of temporary files","unlimited"                                 ),

  CMD_OPTION_INTEGER64    ("archive-part-size",                 's',0,2,globalOptions.archivePartSize,                   0,0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "approximated archive part size","unlimited"                               ),
  CMD_OPTION_INTEGER64    ("fragment-size",                     0,  0,3,globalOptions.fragmentSize,                      0,0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "fragment size","unlimited"                                                ),

  CMD_OPTION_INTEGER      ("directory-strip",                   'p',1,2,globalOptions.directoryStripCount,               0,-1,MAX_INT,NULL,                                             "number of directories to strip on extract",NULL                           ),
  CMD_OPTION_STRING       ("destination",                       0,  0,2,globalOptions.destination,                       0,                                                             "destination to restore entries","path"                                    ),
  CMD_OPTION_SPECIAL      ("owner",                             0,  0,2,&globalOptions.owner,                            0,cmdOptionParseOwner,NULL,1,                                  "user and group of restored files","user:group"                            ),
  CMD_OPTION_SPECIAL      ("permissions",                       0,  0,2,&globalOptions.permissions,                      0,cmdOptionParsePermissions,NULL,1,                            "permissions of restored files","<owner>:<group>:<world>|<number>"         ),

  CMD_OPTION_STRING       ("comment",                           0,  1,1,globalOptions.comment,                           GLOBAL_OPTION_SET_COMMENT,                                        "comment","text"                                                           ),

  CMD_OPTION_CSTRING      ("directory",                         'C',1,0,changeToDirectory,                               0,                                                             "change to directory","path"                                               ),

  CMD_OPTION_SPECIAL      ("compress-algorithm",                'z',0,2,&globalOptions.compressAlgorithms,               GLOBAL_OPTION_SET_COMPRESS_ALGORITHMS,cmdOptionParseCompressAlgorithms,NULL,1,"select compress algorithms to use\n"
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
  CMD_OPTION_INTEGER      ("compress-min-size",                 0,  1,2,globalOptions.compressMinFileSize,               0,0,MAX_INT,COMMAND_LINE_BYTES_UNITS,                          "minimal size of file for compression",NULL                                ),
  CMD_OPTION_SPECIAL      ("compress-exclude",                  0,  0,3,&globalOptions.compressExcludePatternList,       0,cmdOptionParsePattern,NULL,1,                                "exclude compression pattern","pattern"                                    ),

  CMD_OPTION_SPECIAL      ("crypt-algorithm",                   'y',0,2,globalOptions.cryptAlgorithms,                   GLOBAL_OPTION_SET_CRYPT_ALGORITHMS,cmdOptionParseCryptAlgorithms,NULL,1,"select crypt algorithms to use\n"
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
  CMD_OPTION_SELECT       ("crypt-type",                        0,  0,2,globalOptions.cryptType,                         0,COMMAND_LINE_OPTIONS_CRYPT_TYPES,                            "select crypt type"                                                        ),
  CMD_OPTION_SPECIAL      ("crypt-password",                    0,  0,2,&globalOptions.cryptPassword,                    0,cmdOptionParsePassword,NULL,1,                               "crypt password (use with care!)","password"                               ),
  CMD_OPTION_SPECIAL      ("crypt-new-password",                0,  0,2,&globalOptions.cryptNewPassword,                 0,cmdOptionParsePassword,NULL,1,                               "new crypt password (use with care!)","password"                           ),
//#warning remove/revert
  CMD_OPTION_SPECIAL      ("crypt-public-key",                  0,  0,2,&globalOptions.cryptPublicKey,                   0,cmdOptionParseKeyData,NULL,1,                                "public key for asymmetric encryption","file name|data"                    ),
  CMD_OPTION_SPECIAL      ("crypt-private-key",                 0,  0,2,&globalOptions.cryptPrivateKey,                  0,cmdOptionParseKeyData,NULL,1,                                "private key for asymmetric decryption","file name|data"                   ),
  CMD_OPTION_SPECIAL      ("signature-public-key",              0,  0,1,&globalOptions.signaturePublicKey,               0,cmdOptionParsePublicPrivateKey,NULL,1,                       "public key for signature check","file name|data"                          ),
  CMD_OPTION_SPECIAL      ("signature-private-key",             0,  0,2,&globalOptions.signaturePrivateKey,              0,cmdOptionParsePublicPrivateKey,NULL,1,                       "private key for signature generation","file name|data"                    ),

//TODO
//  CMD_OPTION_INTEGER64    ("file-max-storage-size",              0,  0,2,defaultFileServer.maxStorageSize,               0,0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on file server","unlimited"                  ),

  CMD_OPTION_STRING       ("ftp-login-name",                    0,  0,2,defaultFTPServer.ftp.loginName,                  0,                                                             "ftp login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ftp-password",                      0,  0,2,&defaultFTPServer.ftp.password,                  0,cmdOptionParsePassword,NULL,1,                               "ftp password (use with care!)","password"                                 ),
  CMD_OPTION_INTEGER      ("ftp-max-connections",               0,  0,2,defaultFTPServer.maxConnectionCount,             0,0,MAX_INT,NULL,                                              "max. number of concurrent ftp connections","unlimited"                    ),
//TODO
//  CMD_OPTION_INTEGER64    ("ftp-max-storage-size",              0,  0,2,defaultFTPServer.maxStorageSize,                 NULL,0LL,MAX_INT64,NULL,                                       "max. number of bytes to store on ftp server","unlimited"                  ),

  CMD_OPTION_INTEGER      ("ssh-port",                          0,  0,2,defaultSSHServer.ssh.port,                       0,0,65535,NULL,                                                "ssh port",NULL                                                            ),
  CMD_OPTION_STRING       ("ssh-login-name",                    0,  0,2,defaultSSHServer.ssh.loginName,                  0,                                                             "ssh login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ssh-password",                      0,  0,2,&defaultSSHServer.ssh.password,                  0,cmdOptionParsePassword,NULL,1,                               "ssh password (use with care!)","password"                                 ),
  CMD_OPTION_SPECIAL      ("ssh-public-key",                    0,  1,2,&defaultSSHServer.ssh.publicKey,                 0,cmdOptionParseKeyData,NULL,1,                                "ssh public key","file name|data"                                          ),
  CMD_OPTION_SPECIAL      ("ssh-private-key",                   0,  1,2,&defaultSSHServer.ssh.privateKey,                0,cmdOptionParseKeyData,NULL,1,                                "ssh private key","file name|data"                                         ),
  CMD_OPTION_INTEGER      ("ssh-max-connections",               0,  1,2,defaultSSHServer.maxConnectionCount,             0,0,MAX_INT,NULL,                                              "max. number of concurrent ssh connections","unlimited"                    ),
//TODO
//  CMD_OPTION_INTEGER64    ("ssh-max-storage-size",              0,  0,2,defaultSSHServer.maxStorageSize,                 0,0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on ssh server","unlimited"                  ),

//  CMD_OPTION_INTEGER      ("webdav-port",                       0,  0,2,defaultWebDAVServer.webDAV.port,               0,0,65535,NULL,                                                  "WebDAV port",NULL                                                         ),
  CMD_OPTION_STRING       ("webdav-login-name",                 0,  0,2,defaultWebDAVServer.webDAV.loginName,            0,                                                             "WebDAV login name","name"                                                 ),
  CMD_OPTION_SPECIAL      ("webdav-password",                   0,  0,2,&defaultWebDAVServer.webDAV.password,            0,cmdOptionParsePassword,NULL,1,                               "WebDAV password (use with care!)","password"                              ),
  CMD_OPTION_INTEGER      ("webdav-max-connections",            0,  0,2,defaultWebDAVServer.maxConnectionCount,          0,0,MAX_INT,NULL,                                              "max. number of concurrent WebDAV connections","unlimited"                 ),
//TODO
//  CMD_OPTION_INTEGER64    ("webdav-max-storage-size",           0,  0,2,defaultWebDAVServer.maxStorageSize,              0,0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on WebDAV server","unlimited"               ),

  CMD_OPTION_BOOLEAN      ("daemon",                            0,  1,0,daemonFlag,                                      0,                                                             "run in daemon mode"                                                       ),
  CMD_OPTION_BOOLEAN      ("no-detach",                         'D',1,0,noDetachFlag,                                    0,                                                             "do not detach in daemon mode"                                             ),
//  CMD_OPTION_BOOLEAN      ("pair-master",                       0  ,1,0,pairMasterFlag,                                  0,                                                             "pair master"                                                              ),
  CMD_OPTION_SELECT       ("server-mode",                       0,  1,1,serverMode,                                      0,COMMAND_LINE_OPTIONS_SERVER_MODES,                           "select server mode"                                                       ),
  CMD_OPTION_INTEGER      ("server-port",                       0,  1,1,serverPort,                                      0,0,65535,NULL,                                                "server port",NULL                                                         ),
  CMD_OPTION_INTEGER      ("server-tls-port",                   0,  1,1,serverTLSPort,                                   0,0,65535,NULL,                                                "TLS (SSL) server port",NULL                                               ),
  CMD_OPTION_SPECIAL      ("server-ca-file",                    0,  1,1,&serverCA,                                       0,cmdOptionReadCertificateFile,NULL,1,                         "TLS (SSL) server certificate authority file (CA file)","file name"        ),
  CMD_OPTION_SPECIAL      ("server-cert-file",                  0,  1,1,&serverCert,                                     0,cmdOptionReadCertificateFile,NULL,1,                         "TLS (SSL) server certificate file","file name"                            ),
  CMD_OPTION_SPECIAL      ("server-key-file",                   0,  1,1,&serverKey,                                      0,cmdOptionReadKeyFile,NULL,1,                                 "TLS (SSL) server key file","file name"                                    ),
  CMD_OPTION_SPECIAL      ("server-password",                   0,  1,1,&serverPasswordHash,                             0,cmdOptionParseHashData,NULL,1,                               "server password (use with care!)","password"                              ),
  CMD_OPTION_INTEGER      ("server-max-connections",            0,  1,1,serverMaxConnections,                            0,0,65535,NULL,                                                "max. concurrent connections to server",NULL                               ),

  CMD_OPTION_INTEGER      ("nice-level",                        0,  1,1,globalOptions.niceLevel,                         0,0,19,NULL,                                                   "general nice level of processes/threads",NULL                             ),
  CMD_OPTION_INTEGER      ("max-threads",                       0,  1,1,globalOptions.maxThreads,                        0,0,65535,NULL,                                                "max. number of concurrent compress/encryption threads","cpu cores"        ),

  CMD_OPTION_SPECIAL      ("max-band-width",                    0,  1,1,&globalOptions.maxBandWidthList,                 0,cmdOptionParseBandWidth,NULL,1,                              "max. network band width to use [bits/s]","number or file name"            ),

  CMD_OPTION_BOOLEAN      ("batch",                             0,  2,1,batchFlag,                                       0,                                                             "run in batch mode"                                                        ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",             0,  1,1,&globalOptions.remoteBARExecutable,              0,cmdOptionParseString,NULL,1,                                 "remote BAR executable","file name"                                        ),

  CMD_OPTION_STRING       ("pre-command",                       0,  1,1,globalOptions.preProcessScript,                  0,                                                             "pre-process command","command"                                            ),
  CMD_OPTION_STRING       ("post-command",                      0,  1,1,globalOptions.postProcessScript,                 0,                                                             "post-process command","command"                                           ),

  CMD_OPTION_STRING       ("file-write-pre-command",            0,  1,1,globalOptions.file.writePreProcessCommand,       0,                                                             "write file pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("file-write-post-command",           0,  1,1,globalOptions.file.writePostProcessCommand,      0,                                                             "write file post-process command","command"                                ),

  CMD_OPTION_STRING       ("ftp-write-pre-command",             0,  1,1,globalOptions.ftp.writePreProcessCommand,        0,                                                             "write FTP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("ftp-write-post-command",            0,  1,1,globalOptions.ftp.writePostProcessCommand,       0,                                                             "write FTP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("scp-write-pre-command",             0,  1,1,globalOptions.scp.writePreProcessCommand,        0,                                                             "write SCP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("scp-write-post-command",            0,  1,1,globalOptions.scp.writePostProcessCommand,       0,                                                             "write SCP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("sftp-write-pre-command",            0,  1,1,globalOptions.sftp.writePreProcessCommand,       0,                                                             "write SFTP pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("sftp-write-post-command",           0,  1,1,globalOptions.sftp.writePostProcessCommand,      0,                                                             "write SFTP post-process command","command"                                ),

  CMD_OPTION_STRING       ("webdav-write-pre-command",          0,  1,1,globalOptions.webdav.writePreProcessCommand,     0,                                                             "write WebDAV pre-process command","command"                               ),
  CMD_OPTION_STRING       ("webdav-write-post-command",         0,  1,1,globalOptions.webdav.writePostProcessCommand,    0,                                                             "write WebDAV post-process command","command"                              ),

  CMD_OPTION_STRING       ("cd-device",                         0,  1,1,globalOptions.cd.deviceName,                     0,                                                             "default CD device","device name"                                          ),
  CMD_OPTION_STRING       ("cd-request-volume-command",         0,  1,1,globalOptions.cd.requestVolumeCommand,           0,                                                             "request new CD volume command","command"                                  ),
  CMD_OPTION_STRING       ("cd-unload-volume-command",          0,  1,1,globalOptions.cd.unloadVolumeCommand,            0,                                                             "unload CD volume command","command"                                       ),
  CMD_OPTION_STRING       ("cd-load-volume-command",            0,  1,1,globalOptions.cd.loadVolumeCommand,              0,                                                             "load CD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("cd-volume-size",                    0,  1,1,globalOptions.cd.volumeSize,                     0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "CD volume size","unlimited"                                               ),
  CMD_OPTION_STRING       ("cd-image-pre-command",              0,  1,1,globalOptions.cd.imagePreProcessCommand,         0,                                                             "make CD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("cd-image-post-command",             0,  1,1,globalOptions.cd.imagePostProcessCommand,        0,                                                             "make CD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("cd-image-command",                  0,  1,1,globalOptions.cd.imageCommand,                   0,                                                             "make CD image command","command"                                          ),
  CMD_OPTION_STRING       ("cd-ecc-pre-command",                0,  1,1,globalOptions.cd.eccPreProcessCommand,           0,                                                             "make CD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("cd-ecc-post-command",               0,  1,1,globalOptions.cd.eccPostProcessCommand,          0,                                                             "make CD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("cd-ecc-command",                    0,  1,1,globalOptions.cd.eccCommand,                     0,                                                             "make CD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("cd-blank-command",                  0,  1,1,globalOptions.cd.blankCommand,                   0,                                                             "blank CD medium command","command"                                        ),
  CMD_OPTION_STRING       ("cd-write-pre-command",              0,  1,1,globalOptions.cd.writePreProcessCommand,         0,                                                             "write CD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("cd-write-post-command",             0,  1,1,globalOptions.cd.writePostProcessCommand,        0,                                                             "write CD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("cd-write-command",                  0,  1,1,globalOptions.cd.writeCommand,                   0,                                                             "write CD command","command"                                               ),
  CMD_OPTION_STRING       ("cd-write-image-command",            0,  1,1,globalOptions.cd.writeImageCommand,              0,                                                             "write CD image command","command"                                         ),

  CMD_OPTION_STRING       ("dvd-device",                        0,  1,1,globalOptions.dvd.deviceName,                    0,                                                             "default DVD device","device name"                                         ),
  CMD_OPTION_STRING       ("dvd-request-volume-command",        0,  1,1,globalOptions.dvd.requestVolumeCommand,          0,                                                             "request new DVD volume command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-unload-volume-command",         0,  1,1,globalOptions.dvd.unloadVolumeCommand,           0,                                                             "unload DVD volume command","command"                                      ),
  CMD_OPTION_STRING       ("dvd-load-volume-command",           0,  1,1,globalOptions.dvd.loadVolumeCommand,             0,                                                             "load DVD volume command","command"                                        ),
  CMD_OPTION_INTEGER64    ("dvd-volume-size",                   0,  1,1,globalOptions.dvd.volumeSize,                    0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "DVD volume size","unlimited"                                              ),
  CMD_OPTION_STRING       ("dvd-image-pre-command",             0,  1,1,globalOptions.dvd.imagePreProcessCommand,        0,                                                             "make DVD image pre-process command","command"                             ),
  CMD_OPTION_STRING       ("dvd-image-post-command",            0,  1,1,globalOptions.dvd.imagePostProcessCommand,       0,                                                             "make DVD image post-process command","command"                            ),
  CMD_OPTION_STRING       ("dvd-image-command",                 0,  1,1,globalOptions.dvd.imageCommand,                  0,                                                             "make DVD image command","command"                                         ),
  CMD_OPTION_STRING       ("dvd-ecc-pre-command",               0,  1,1,globalOptions.dvd.eccPreProcessCommand,          0,                                                             "make DVD error-correction codes pre-process command","command"            ),
  CMD_OPTION_STRING       ("dvd-ecc-post-command",              0,  1,1,globalOptions.dvd.eccPostProcessCommand,         0,                                                             "make DVD error-correction codes post-process command","command"           ),
  CMD_OPTION_STRING       ("dvd-ecc-command",                   0,  1,1,globalOptions.dvd.eccCommand,                    0,                                                             "make DVD error-correction codes command","command"                        ),
  CMD_OPTION_STRING       ("dvd-blank-command",                 0,  1,1,globalOptions.dvd.blankCommand,                  0,                                                             "blank DVD mediumcommand","command"                                        ),
  CMD_OPTION_STRING       ("dvd-write-pre-command",             0,  1,1,globalOptions.dvd.writePreProcessCommand,        0,                                                             "write DVD pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("dvd-write-post-command",            0,  1,1,globalOptions.dvd.writePostProcessCommand,       0,                                                             "write DVD post-process command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-write-command",                 0,  1,1,globalOptions.dvd.writeCommand,                  0,                                                             "write DVD command","command"                                              ),
  CMD_OPTION_STRING       ("dvd-write-image-command",           0,  1,1,globalOptions.dvd.writeImageCommand,             0,                                                             "write DVD image command","command"                                        ),

  CMD_OPTION_STRING       ("bd-device",                         0,  1,1,globalOptions.bd.deviceName,                     0,                                                             "default BD device","device name"                                          ),
  CMD_OPTION_STRING       ("bd-request-volume-command",         0,  1,1,globalOptions.bd.requestVolumeCommand,           0,                                                             "request new BD volume command","command"                                  ),
  CMD_OPTION_STRING       ("bd-unload-volume-command",          0,  1,1,globalOptions.bd.unloadVolumeCommand,            0,                                                             "unload BD volume command","command"                                       ),
  CMD_OPTION_STRING       ("bd-load-volume-command",            0,  1,1,globalOptions.bd.loadVolumeCommand,              0,                                                             "load BD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("bd-volume-size",                    0,  1,1,globalOptions.bd.volumeSize,                     0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "BD volume size","unlimited"                                               ),
  CMD_OPTION_STRING       ("bd-image-pre-command",              0,  1,1,globalOptions.bd.imagePreProcessCommand,         0,                                                             "make BD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("bd-image-post-command",             0,  1,1,globalOptions.bd.imagePostProcessCommand,        0,                                                             "make BD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("bd-image-command",                  0,  1,1,globalOptions.bd.imageCommand,                   0,                                                             "make BD image command","command"                                          ),
  CMD_OPTION_STRING       ("bd-ecc-pre-command",                0,  1,1,globalOptions.bd.eccPreProcessCommand,           0,                                                             "make BD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("bd-ecc-post-command",               0,  1,1,globalOptions.bd.eccPostProcessCommand,          0,                                                             "make BD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("bd-ecc-command",                    0,  1,1,globalOptions.bd.eccCommand,                     0,                                                             "make BD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("bd-blank-command",                  0,  1,1,globalOptions.bd.blankCommand,                   0,                                                             "blank BD medium command","command"                                        ),
  CMD_OPTION_STRING       ("bd-write-pre-command",              0,  1,1,globalOptions.bd.writePreProcessCommand,         0,                                                             "write BD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("bd-write-post-command",             0,  1,1,globalOptions.bd.writePostProcessCommand,        0,                                                             "write BD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("bd-write-command",                  0,  1,1,globalOptions.bd.writeCommand,                   0,                                                             "write BD command","command"                                               ),
  CMD_OPTION_STRING       ("bd-write-image-command",            0,  1,1,globalOptions.bd.writeImageCommand,              0,                                                             "write BD image command","command"                                         ),

  CMD_OPTION_STRING       ("device",                            0,  1,1,defaultDevice.name,                              0,                                                             "default device","device name"                                             ),
  CMD_OPTION_STRING       ("device-request-volume-command",     0,  1,1,defaultDevice.requestVolumeCommand,              0,                                                             "request new volume command","command"                                     ),
  CMD_OPTION_STRING       ("device-load-volume-command",        0,  1,1,defaultDevice.loadVolumeCommand,                 0,                                                             "load volume command","command"                                            ),
  CMD_OPTION_STRING       ("device-unload-volume-command",      0,  1,1,defaultDevice.unloadVolumeCommand,               0,                                                             "unload volume command","command"                                          ),
  CMD_OPTION_INTEGER64    ("device-volume-size",                0,  1,1,defaultDevice.volumeSize,                        0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "volume size","unlimited"                                                  ),
  CMD_OPTION_STRING       ("device-image-pre-command",          0,  1,1,defaultDevice.imagePreProcessCommand,            0,                                                             "make image pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("device-image-post-command",         0,  1,1,defaultDevice.imagePostProcessCommand,           0,                                                             "make image post-process command","command"                                ),
  CMD_OPTION_STRING       ("device-image-command",              0,  1,1,defaultDevice.imageCommand,                      0,                                                             "make image command","command"                                             ),
  CMD_OPTION_STRING       ("device-ecc-pre-command",            0,  1,1,defaultDevice.eccPreProcessCommand,              0,                                                             "make error-correction codes pre-process command","command"                ),
  CMD_OPTION_STRING       ("device-ecc-post-command",           0,  1,1,defaultDevice.eccPostProcessCommand,             0,                                                             "make error-correction codes post-process command","command"               ),
  CMD_OPTION_STRING       ("device-ecc-command",                0,  1,1,defaultDevice.eccCommand,                        0,                                                             "make error-correction codes command","command"                            ),
  CMD_OPTION_STRING       ("device-blank-command",              0,  1,1,defaultDevice.blankCommand,                      0,                                                             "blank device medium command","command"                                    ),
  CMD_OPTION_STRING       ("device-write-pre-command",          0,  1,1,defaultDevice.writePreProcessCommand,            0,                                                             "write device pre-process command","command"                               ),
  CMD_OPTION_STRING       ("device-write-post-command",         0,  1,1,defaultDevice.writePostProcessCommand,           0,                                                             "write device post-process command","command"                              ),
  CMD_OPTION_STRING       ("device-write-command",              0,  1,1,defaultDevice.writeCommand,                      0,                                                             "write device command","command"                                           ),

  CMD_OPTION_INTEGER64    ("max-storage-size",                  0,  1,2,globalOptions.maxStorageSize,                    0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "max. storage size","unlimited"                                            ),
  CMD_OPTION_INTEGER64    ("volume-size",                       0,  1,2,globalOptions.volumeSize,                        0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "volume size","unlimited"                                                  ),
  CMD_OPTION_BOOLEAN      ("ecc",                               0,  1,2,globalOptions.errorCorrectionCodesFlag,          0,                                                             "add error-correction codes with 'dvdisaster' tool"                        ),
  CMD_OPTION_BOOLEAN      ("always-create-image",               0,  1,2,globalOptions.alwaysCreateImageFlag,             0,                                                             "always create image for CD/DVD/BD/device"                                 ),
  CMD_OPTION_BOOLEAN      ("blank",                             0,  1,2,globalOptions.blankFlag,                         0,                                                             "blank medium before writing"                                              ),

  CMD_OPTION_STRING       ("jobs-directory",                    0,  1,1,globalOptions.jobsDirectory,                     0,                                                             "server job directory","path name"                                         ),
  CMD_OPTION_STRING       ("incremental-data-directory",        0,  1,1,globalOptions.incrementalDataDirectory,          0,                                                             "server incremental data directory","path name"                            ),

  CMD_OPTION_CSTRING      ("index-database",                    0,  1,1,indexDatabaseFileName,                           0,                                                             "index database file name","file name"                                     ),
  CMD_OPTION_BOOLEAN      ("index-database-auto-update",        0,  1,1,globalOptions.indexDatabaseAutoUpdateFlag,       0,                                                             "enabled automatic update index database"                                  ),
  CMD_OPTION_SPECIAL      ("index-database-max-band-width",     0,  1,1,&globalOptions.indexDatabaseMaxBandWidthList,    0,cmdOptionParseBandWidth,NULL,1,                              "max. band width to use for index updates [bis/s]","number or file name"   ),
  CMD_OPTION_INTEGER      ("index-database-keep-time",          0,  1,1,globalOptions.indexDatabaseKeepTime,             0,0,MAX_INT,COMMAND_LINE_TIME_UNITS,                           "time to keep index data of not existing storages",NULL                    ),

  CMD_OPTION_CSTRING      ("continuous-database",               0,  2,1,continuousDatabaseFileName,                      0,                                                             "continuous database file name (default: in memory)","file name"           ),
  CMD_OPTION_INTEGER64    ("continuous-max-size",               0,  1,2,globalOptions.continuousMaxSize,                 0,0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "max. continuous size","unlimited"                                         ),
  CMD_OPTION_INTEGER      ("continuous-min-time-delta",         0,  1,1,globalOptions.continuousMinTimeDelta,            0,0,MAX_INT,COMMAND_LINE_TIME_UNITS,                           "min. time between continuous backup of an entry",NULL                     ),

  CMD_OPTION_SET          ("log",                               0,  1,1,logTypes,                                        0,COMMAND_LINE_OPTIONS_LOG_TYPES,                              "log types"                                                                ),
  CMD_OPTION_CSTRING      ("log-file",                          0,  1,1,logFileName,                                     0,                                                             "log file name","file name"                                                ),
  CMD_OPTION_CSTRING      ("log-format",                        0,  1,1,logFormat,                                       0,                                                             "log format","format"                                                      ),
  CMD_OPTION_CSTRING      ("log-post-command",                  0,  1,1,logPostCommand,                                  0,                                                             "log file post-process command","command"                                  ),

  CMD_OPTION_CSTRING      ("pid-file",                          0,  1,1,pidFileName,                                     0,                                                             "process id file name","file name"                                         ),

  CMD_OPTION_CSTRING      ("pairing-master-file",               0,  1,1,globalOptions.masterInfo.pairingFileName,        0,                                                             "pairing master enable file name","file name"                              ),

  CMD_OPTION_BOOLEAN      ("info",                              0  ,0,1,globalOptions.metaInfoFlag,                      0,                                                             "show meta info"                                                           ),

  CMD_OPTION_BOOLEAN      ("group",                             'g',0,1,globalOptions.groupFlag,                         0,                                                             "group files in list"                                                      ),
  CMD_OPTION_BOOLEAN      ("all",                               0,  0,1,globalOptions.allFlag,                           0,                                                             "show all files"                                                           ),
  CMD_OPTION_BOOLEAN      ("long-format",                       'L',0,1,globalOptions.longFormatFlag,                    0,                                                             "list in long format"                                                      ),
  CMD_OPTION_BOOLEAN      ("human-format",                      'H',0,1,globalOptions.humanFormatFlag,                   0,                                                             "list in human readable format"                                            ),
  CMD_OPTION_BOOLEAN      ("numeric-uid-gid",                   0,  0,1,globalOptions.numericUIDGIDFlag,                 0,                                                             "print numeric user/group ids"                                             ),
  CMD_OPTION_BOOLEAN      ("numeric-permissions",               0,  0,1,globalOptions.numericPermissionsFlag,            0,                                                             "print numeric file/directory permissions"                                 ),
  CMD_OPTION_BOOLEAN      ("no-header-footer",                  0,  0,1,globalOptions.noHeaderFooterFlag,                0,                                                             "no header/footer output in list"                                          ),
  CMD_OPTION_BOOLEAN      ("delete-old-archive-files",          0,  1,1,globalOptions.deleteOldArchiveFilesFlag,         0,                                                             "delete old archive files after creating new files"                        ),
  CMD_OPTION_BOOLEAN      ("ignore-no-backup-file",             0,  1,2,globalOptions.ignoreNoBackupFileFlag,            0,                                                             "ignore .nobackup/.NOBACKUP file"                                          ),
  CMD_OPTION_BOOLEAN      ("ignore-no-dump",                    0,  1,2,globalOptions.ignoreNoDumpAttributeFlag,         0,                                                             "ignore 'no dump' attribute of files"                                      ),

  CMD_OPTION_BOOLEAN      ("skip-unreadable",                   0,  0,2,globalOptions.skipUnreadableFlag,                0,                                                             "skip unreadable files"                                                    ),
  CMD_OPTION_BOOLEAN      ("force-delta-compression",           0,  0,2,globalOptions.forceDeltaCompressionFlag,         0,                                                             "force delta compression of files. Stop on error"                          ),
  CMD_OPTION_BOOLEAN      ("raw-images",                        0,  1,2,globalOptions.rawImagesFlag,                     0,                                                             "store raw images (store all image blocks)"                                ),
  CMD_OPTION_BOOLEAN      ("no-fragments-check",                0,  1,2,globalOptions.noFragmentsCheckFlag,              0,                                                             "do not check completeness of file fragments"                              ),
  CMD_OPTION_BOOLEAN      ("no-index-database",                 0,  1,1,globalOptions.noIndexDatabaseFlag,               0,                                                             "do not store index database for archives"                                 ),
  CMD_OPTION_SELECT       ("archive-file-mode",                 0,  1,2,globalOptions.archiveFileMode,                   0,COMMAND_LINE_OPTIONS_ARCHIVE_FILE_MODES,                     "select archive files write mode"                                          ),
  // Note: shortcut for --archive-file-mode=overwrite
  CMD_OPTION_SPECIAL      ("overwrite-archive-files",           'o',0,2,&globalOptions.archiveFileMode,                  0,cmdOptionParseArchiveFileModeOverwrite,NULL,0,               "overwrite existing archive files",""                                      ),
  CMD_OPTION_SELECT       ("restore-entry-mode",                0,  1,2,globalOptions.restoreEntryMode,                  0,COMMAND_LINE_OPTIONS_RESTORE_ENTRY_MODES,                    "restore entry mode"                                                       ),
  // Note: shortcut for --restore-entry-mode=overwrite
  CMD_OPTION_SPECIAL      ("overwrite-files",                   0,  0,2,&globalOptions.restoreEntryMode,                 0,cmdOptionParseRestoreEntryModeOverwrite,NULL,0,              "overwrite existing entries on restore",""                                 ),
  CMD_OPTION_BOOLEAN      ("wait-first-volume",                 0,  1,2,globalOptions.waitFirstVolumeFlag,               0,                                                             "wait for first volume"                                                    ),
  CMD_OPTION_BOOLEAN      ("no-signature",                      0  ,1,2,globalOptions.noSignatureFlag,                   0,                                                             "do not create signatures"                                                 ),
  CMD_OPTION_BOOLEAN      ("skip-verify-signatures",            0,  0,2,globalOptions.skipVerifySignaturesFlag,          0,                                                             "do not verify signatures of archives"                                     ),
  CMD_OPTION_BOOLEAN      ("force-verify-signatures",           0,  0,2,globalOptions.forceVerifySignaturesFlag,         0,                                                             "force verify signatures of archives. Stop on error"                       ),
  CMD_OPTION_BOOLEAN      ("no-bar-on-medium",                  0,  1,2,globalOptions.noBAROnMediumFlag,                 0,                                                             "do not store a copy of BAR on medium"                                     ),
  CMD_OPTION_BOOLEAN      ("no-stop-on-error",                  0,  1,2,globalOptions.noStopOnErrorFlag,                 0,                                                             "do not immediately stop on error"                                         ),
  CMD_OPTION_BOOLEAN      ("no-stop-on-attribute-error",        0,  1,2,globalOptions.noStopOnAttributeErrorFlag,        0,                                                             "do not immediately stop on attribute error"                               ),

  CMD_OPTION_SPECIAL      ("no-storage",                        0,  1,2,&storageFlags,                                   0,cmdOptionParseStorageFlagNoStorage,NULL,0,                   "do not store archives (skip storage, index database",""                   ),
  CMD_OPTION_SPECIAL      ("dry-run",                           0,  1,2,&storageFlags,                                   0,cmdOptionParseStorageFlagDryRun,NULL,0,                      "do dry-run (skip storage/restore, incremental data, index database)",""   ),

  CMD_OPTION_BOOLEAN      ("no-default-config",                 0,  1,1,globalOptions.noDefaultConfigFlag,               0,                                                             "do not read configuration files " CONFIG_DIR "/bar.cfg and ~/.bar/" DEFAULT_CONFIG_FILE_NAME),
  CMD_OPTION_BOOLEAN      ("quiet",                             0,  1,1,globalOptions.quietFlag,                         0,                                                             "suppress any output"                                                      ),
  CMD_OPTION_INCREMENT    ("verbose",                           'v',0,0,globalOptions.verboseLevel,                      0,0,6,                                                         "increment/set verbosity level"                                            ),

  CMD_OPTION_BOOLEAN      ("version",                           0  ,0,0,versionFlag,                                     0,                                                             "output version"                                                           ),
  CMD_OPTION_BOOLEAN      ("help",                              'h',0,0,helpFlag,                                        0,                                                             "output this help"                                                         ),
  CMD_OPTION_BOOLEAN      ("xhelp",                             0,  0,0,xhelpFlag,                                       0,                                                             "output help to extended options"                                          ),
  CMD_OPTION_BOOLEAN      ("help-internal",                     0,  1,0,helpInternalFlag,                                0,                                                             "output help to internal options"                                          ),

  // deprecated
  CMD_OPTION_DEPRECATED   ("server-jobs-directory",             0,  1,1,&globalOptions.jobsDirectory,                    0,CmdOptionParseDeprecatedStringOption,NULL,1,                 "jobs-directory"                                                           ),
  CMD_OPTION_DEPRECATED   ("mount-device",                      0,  1,2,&globalOptions.mountList,                        0,cmdOptionParseDeprecatedMountDevice,NULL,1,                  "device to mount/unmount"                                                  ),
  CMD_OPTION_DEPRECATED   ("stop-on-error",                     0,  1,2,&globalOptions.noStopOnErrorFlag,                0,cmdOptionParseDeprecatedStopOnError,NULL,0,                  "no-stop-on-error"                                                         ),

  CMD_OPTION_INCREMENT    ("server-debug",                      0,  2,1,globalOptions.serverDebugLevel,                  0,0,2,                                                         "debug level for server"                                                   ),
  CMD_OPTION_BOOLEAN      ("server-debug-index-operations",     0,  2,1,globalOptions.serverDebugIndexOperationsFlag,    0,                                                             "run server index operations only"                                         ),
};

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

// handle deprecated configuration values
LOCAL bool configValueParseDeprecatedArchiveFileModeOverwrite(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedRestoreEntryModeOverwrite(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedMountDevice(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedStopOnError(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

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

ConfigValue CONFIG_VALUES[] = CONFIG_VALUE_ARRAY
(
  // general settings
  CONFIG_VALUE_STRING            ("UUID",                             &uuid,-1                                                       ),

  CONFIG_VALUE_SPECIAL           ("config",                           &configFileNameList,-1,                                        configValueParseConfigFile,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_BEGIN_SECTION     ("master",-1),
//TODO
    CONFIG_VALUE_STRING          ("name",                             &globalOptions.masterInfo.name,-1                              ),
    CONFIG_VALUE_SPECIAL         ("uuid-hash",                        &globalOptions.masterInfo.uuidHash,-1,                         configValueParseHashData,configValueFormatInitHashData,configValueFormatDoneHashData,configValueFormatHashData,NULL),
//TODO: required to save?
    CONFIG_VALUE_SPECIAL         ("public-key",                       &globalOptions.masterInfo.publicKey,-1,                        configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_STRING            ("tmp-directory",                    &globalOptions.tmpDirectory,-1                                 ),
  CONFIG_VALUE_INTEGER64         ("max-tmp-size",                     &globalOptions.maxTmpSize,-1,                                  0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER           ("nice-level",                       &globalOptions.niceLevel,-1,                                   0,19,NULL),
  CONFIG_VALUE_INTEGER           ("max-threads",                      &globalOptions.maxThreads,-1,                                  0,65535,NULL),

  CONFIG_VALUE_SPECIAL           ("max-band-width",                   &globalOptions.maxBandWidthList,-1,                            configValueParseBandWidth,NULL,NULL,NULL,&globalOptions.maxBandWidthList),

  CONFIG_VALUE_INTEGER           ("compress-min-size",                &globalOptions.compressMinFileSize,-1,                         0,MAX_INT,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL           ("compress-exclude",                 &globalOptions.compressExcludePatternList,-1,                  configValueParsePattern,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_STRING            ("jobs-directory",                   &globalOptions.jobsDirectory,-1                                ),
  CONFIG_VALUE_STRING            ("incremental-data-directory",       &globalOptions.incrementalDataDirectory,-1                     ),

  CONFIG_VALUE_CSTRING           ("index-database",                   &indexDatabaseFileName,-1                                      ),
  CONFIG_VALUE_BOOLEAN           ("index-database-auto-update",       &globalOptions.indexDatabaseAutoUpdateFlag,-1                  ),
  CONFIG_VALUE_SPECIAL           ("index-database-max-band-width",    &globalOptions.indexDatabaseMaxBandWidthList,-1,               configValueParseBandWidth,NULL,NULL,NULL,&globalOptions.indexDatabaseMaxBandWidthList),
  CONFIG_VALUE_INTEGER           ("index-database-keep-time",         &globalOptions.indexDatabaseKeepTime,-1,                       0,MAX_INT,CONFIG_VALUE_TIME_UNITS),

  CONFIG_VALUE_CSTRING           ("continuous-database",              &continuousDatabaseFileName,-1                                 ),
  CONFIG_VALUE_INTEGER64         ("continuous-max-size",              &globalOptions.continuousMaxSize,-1,                           0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_INTEGER           ("continuous-min-time-delta",        &globalOptions.continuousMinTimeDelta,-1,                      0,MAX_INT,CONFIG_VALUE_TIME_UNITS),

  // global job settings
  CONFIG_VALUE_IGNORE            ("host-name",                                                                                       NULL,FALSE),
  CONFIG_VALUE_IGNORE            ("host-port",                                                                                       NULL,FALSE),
  CONFIG_VALUE_STRING            ("archive-name",                     &storageName,-1                                                ),
  CONFIG_VALUE_SELECT            ("archive-type",                     &globalOptions.archiveType,-1,                                 CONFIG_VALUE_ARCHIVE_TYPES),

  CONFIG_VALUE_STRING            ("incremental-list-file",            &globalOptions.incrementalListFileName,-1                      ),

  CONFIG_VALUE_INTEGER64         ("archive-part-size",                &globalOptions.archivePartSize,-1,                             0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER           ("directory-strip",                  &globalOptions.directoryStripCount,-1,                         -1,MAX_INT,NULL),
  CONFIG_VALUE_STRING            ("destination",                      &globalOptions.destination,-1                                  ),
  CONFIG_VALUE_SPECIAL           ("owner",                            &globalOptions.owner,-1,                                       configValueParseOwner,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("permissions",                      &globalOptions.permissions,-1,                                 configValueParsePermissions,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_SELECT            ("pattern-type",                     &globalOptions.patternType,-1,                                 CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_VALUE_SPECIAL           ("compress-algorithm",               &globalOptions.compressAlgorithms,-1,                          configValueParseCompressAlgorithms,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_SPECIAL           ("crypt-algorithm",                  &globalOptions.cryptAlgorithms,-1,                             configValueParseCryptAlgorithms,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SELECT            ("crypt-type",                       &globalOptions.cryptType,-1,                                   CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_VALUE_SELECT            ("crypt-password-mode",              &globalOptions.cryptPasswordMode,-1,                           CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_VALUE_SPECIAL           ("crypt-password",                   &globalOptions.cryptPassword,-1,                               configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_SPECIAL           ("crypt-public-key",                 &globalOptions.cryptPublicKey,-1,                              configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("crypt-private-key",                &globalOptions.cryptPrivateKey,-1,                             configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("signature-public-key",             &globalOptions.signaturePublicKey,-1,                          configValueParsePublicPrivateKey,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("signature-private-key",            &globalOptions.signaturePrivateKey,-1,                         configValueParsePublicPrivateKey,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_SPECIAL           ("include-file",                     &includeEntryList,-1,                                          configValueParseFileEntryPattern,NULL,NULL,NULL,&globalOptions.patternType),
  CONFIG_VALUE_STRING            ("include-file-list",                &globalOptions.includeFileListFileName,-1                      ),
  CONFIG_VALUE_STRING            ("include-file-command",             &globalOptions.includeFileCommand,-1                           ),
  CONFIG_VALUE_SPECIAL           ("include-image",                    &includeEntryList,-1,                                          configValueParseImageEntryPattern,NULL,NULL,NULL,&globalOptions.patternType),
  CONFIG_VALUE_STRING            ("include-image-list",               &globalOptions.includeImageListFileName,-1                     ),
  CONFIG_VALUE_STRING            ("include-image-command",            &globalOptions.includeImageCommand,-1                          ),
  CONFIG_VALUE_SPECIAL           ("exclude",                          &excludePatternList,-1,                                        configValueParsePattern,NULL,NULL,NULL,&globalOptions.patternType),
  CONFIG_VALUE_STRING            ("exclude-list",                     &globalOptions.excludeListFileName,-1                          ),
  CONFIG_VALUE_STRING            ("exclude-command",                  &globalOptions.excludeCommand,-1                               ),

  CONFIG_VALUE_SPECIAL           ("mount",                            &globalOptions.mountList,-1,                                   configValueParseMount,configValueFormatInitMount,configValueFormatDoneMount,configValueFormatMount,NULL),
  CONFIG_VALUE_STRING            ("mount-command",                    &globalOptions.mountCommand,-1                                 ),
  CONFIG_VALUE_STRING            ("mount-device-command",             &globalOptions.mountDeviceCommand,-1                           ),
  CONFIG_VALUE_STRING            ("unmount-command",                  &globalOptions.unmountCommand,-1                               ),

  CONFIG_VALUE_STRING            ("comment",                          &globalOptions.comment,-1                                      ),

  CONFIG_VALUE_SPECIAL           ("delta-source",                     &globalOptions.deltaSourceList,-1,                             configValueParseDeltaSource,NULL,NULL,NULL,&globalOptions.patternType),

  CONFIG_VALUE_INTEGER64         ("max-storage-size",                 &globalOptions.maxStorageSize,-1,                              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_INTEGER64         ("volume-size",                      &globalOptions.volumeSize,-1,                                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_BOOLEAN           ("ecc",                              &globalOptions.errorCorrectionCodesFlag,-1                     ),
  CONFIG_VALUE_BOOLEAN           ("always-create-image",              &globalOptions.alwaysCreateImageFlag,-1                        ),
  CONFIG_VALUE_BOOLEAN           ("blank",                            &globalOptions.blankFlag,-1                                    ),

  CONFIG_VALUE_BOOLEAN           ("skip-unreadable",                  &globalOptions.skipUnreadableFlag,-1                           ),
  CONFIG_VALUE_BOOLEAN           ("raw-images",                       &globalOptions.rawImagesFlag,-1                                ),
  CONFIG_VALUE_BOOLEAN           ("no-fragments-check",               &globalOptions.noFragmentsCheckFlag,-1                         ),
  CONFIG_VALUE_SELECT            ("archive-file-mode",                &globalOptions.archiveFileMode,-1,                             CONFIG_VALUE_ARCHIVE_FILE_MODES),
  CONFIG_VALUE_SELECT            ("restore-entry-mode",               &globalOptions.restoreEntryMode,-1,                            CONFIG_VALUE_RESTORE_ENTRY_MODES),
  CONFIG_VALUE_BOOLEAN           ("wait-first-volume",                &globalOptions.waitFirstVolumeFlag,-1                          ),
  CONFIG_VALUE_BOOLEAN           ("no-signature",                     &globalOptions.noSignatureFlag,-1                              ),
  CONFIG_VALUE_BOOLEAN           ("skip-verify-signatures",           &globalOptions.skipVerifySignaturesFlag,-1                     ),
  CONFIG_VALUE_BOOLEAN           ("no-bar-on-medium",                 &globalOptions.noBAROnMediumFlag,-1                            ),
  CONFIG_VALUE_BOOLEAN           ("no-stop-on-error",                 &globalOptions.noStopOnErrorFlag,-1                            ),
  CONFIG_VALUE_BOOLEAN           ("no-stop-on-attribute-error",       &globalOptions.noStopOnAttributeErrorFlag,-1                   ),
  CONFIG_VALUE_BOOLEAN           ("quiet",                            &globalOptions.quietFlag,-1                                    ),
  CONFIG_VALUE_INTEGER           ("verbose",                          &globalOptions.verboseLevel,-1,                                0,6,NULL),

  // ignored schedule settings (server only)
  CONFIG_VALUE_BEGIN_SECTION     ("schedule",-1),
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
  CONFIG_VALUE_BEGIN_SECTION     ("persistence",-1),
    CONFIG_VALUE_IGNORE          ("min-keep",                                                                                        NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("max-keep",                                                                                        NULL,FALSE),
    CONFIG_VALUE_IGNORE          ("max-age",                                                                                         NULL,FALSE),
  CONFIG_VALUE_END_SECTION(),

  // commands
  CONFIG_VALUE_STRING            ("pre-command",                      &globalOptions.preProcessScript,-1                             ),
  CONFIG_VALUE_STRING            ("post-command",                     &globalOptions.postProcessScript,-1                            ),

  CONFIG_VALUE_STRING            ("file-write-pre-command",           &globalOptions.file.writePreProcessCommand,-1                  ),
  CONFIG_VALUE_STRING            ("file-write-post-command",          &globalOptions.file.writePostProcessCommand,-1                 ),

  CONFIG_VALUE_STRING            ("ftp-write-pre-command",            &globalOptions.ftp.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("ftp-write-post-command",           &globalOptions.ftp.writePostProcessCommand,-1                  ),

  CONFIG_VALUE_STRING            ("scp-write-pre-command",            &globalOptions.scp.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("scp-write-post-command",           &globalOptions.scp.writePostProcessCommand,-1                  ),

  CONFIG_VALUE_STRING            ("sftp-write-pre-command",           &globalOptions.sftp.writePreProcessCommand,-1                  ),
  CONFIG_VALUE_STRING            ("sftp-write-post-command",          &globalOptions.sftp.writePostProcessCommand,-1                 ),

  CONFIG_VALUE_STRING            ("webdav-write-pre-command",         &globalOptions.webdav.writePreProcessCommand,-1                ),
  CONFIG_VALUE_STRING            ("webdav-write-post-command",        &globalOptions.webdav.writePostProcessCommand,-1               ),

  CONFIG_VALUE_STRING            ("cd-device",                        &globalOptions.cd.deviceName,-1                                ),
  CONFIG_VALUE_STRING            ("cd-request-volume-command",        &globalOptions.cd.requestVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING            ("cd-unload-volume-command",         &globalOptions.cd.unloadVolumeCommand,-1                       ),
  CONFIG_VALUE_STRING            ("cd-load-volume-command",           &globalOptions.cd.loadVolumeCommand,-1                         ),
  CONFIG_VALUE_INTEGER64         ("cd-volume-size",                   &globalOptions.cd.volumeSize,-1,                               0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING            ("cd-image-pre-command",             &globalOptions.cd.imagePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("cd-image-post-command",            &globalOptions.cd.imagePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("cd-image-command",                 &globalOptions.cd.imageCommand,-1                              ),
  CONFIG_VALUE_STRING            ("cd-ecc-pre-command",               &globalOptions.cd.eccPreProcessCommand,-1                      ),
  CONFIG_VALUE_STRING            ("cd-ecc-post-command",              &globalOptions.cd.eccPostProcessCommand,-1                     ),
  CONFIG_VALUE_STRING            ("cd-ecc-command",                   &globalOptions.cd.eccCommand,-1                                ),
  CONFIG_VALUE_STRING            ("cd-blank-command",                 &globalOptions.cd.blankCommand,-1                              ),
  CONFIG_VALUE_STRING            ("cd-write-pre-command",             &globalOptions.cd.writePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("cd-write-post-command",            &globalOptions.cd.writePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("cd-write-command",                 &globalOptions.cd.writeCommand,-1                              ),
  CONFIG_VALUE_STRING            ("cd-write-image-command",           &globalOptions.cd.writeImageCommand,-1                         ),

  CONFIG_VALUE_STRING            ("dvd-device",                       &globalOptions.dvd.deviceName,-1                               ),
  CONFIG_VALUE_STRING            ("dvd-request-volume-command",       &globalOptions.dvd.requestVolumeCommand,-1                     ),
  CONFIG_VALUE_STRING            ("dvd-unload-volume-command",        &globalOptions.dvd.unloadVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING            ("dvd-load-volume-command",          &globalOptions.dvd.loadVolumeCommand,-1                        ),
  CONFIG_VALUE_INTEGER64         ("dvd-volume-size",                  &globalOptions.dvd.volumeSize,-1,                              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING            ("dvd-image-pre-command",            &globalOptions.dvd.imagePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("dvd-image-post-command",           &globalOptions.dvd.imagePostProcessCommand,-1                  ),
  CONFIG_VALUE_STRING            ("dvd-image-command",                &globalOptions.dvd.imageCommand,-1                             ),
  CONFIG_VALUE_STRING            ("dvd-ecc-pre-command",              &globalOptions.dvd.eccPreProcessCommand,-1                     ),
  CONFIG_VALUE_STRING            ("dvd-ecc-post-command",             &globalOptions.dvd.eccPostProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("dvd-ecc-command",                  &globalOptions.dvd.eccCommand,-1                               ),
  CONFIG_VALUE_STRING            ("dvd-blank-command",                &globalOptions.dvd.blankCommand,-1                             ),
  CONFIG_VALUE_STRING            ("dvd-write-pre-command",            &globalOptions.dvd.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("dvd-write-post-command",           &globalOptions.dvd.writePostProcessCommand,-1                  ),
  CONFIG_VALUE_STRING            ("dvd-write-command",                &globalOptions.dvd.writeCommand,-1                             ),
  CONFIG_VALUE_STRING            ("dvd-write-image-command",          &globalOptions.dvd.writeImageCommand,-1                        ),

  CONFIG_VALUE_STRING            ("bd-device",                        &globalOptions.bd.deviceName,-1                                ),
  CONFIG_VALUE_STRING            ("bd-request-volume-command",        &globalOptions.bd.requestVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING            ("bd-unload-volume-command",         &globalOptions.bd.unloadVolumeCommand,-1                       ),
  CONFIG_VALUE_STRING            ("bd-load-volume-command",           &globalOptions.bd.loadVolumeCommand,-1                         ),
  CONFIG_VALUE_INTEGER64         ("bd-volume-size",                   &globalOptions.bd.volumeSize,-1,                               0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING            ("bd-image-pre-command",             &globalOptions.bd.imagePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("bd-image-post-command",            &globalOptions.bd.imagePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("bd-image-command",                 &globalOptions.bd.imageCommand,-1                              ),
  CONFIG_VALUE_STRING            ("bd-ecc-pre-command",               &globalOptions.bd.eccPreProcessCommand,-1                      ),
  CONFIG_VALUE_STRING            ("bd-ecc-post-command",              &globalOptions.bd.eccPostProcessCommand,-1                     ),
  CONFIG_VALUE_STRING            ("bd-ecc-command",                   &globalOptions.bd.eccCommand,-1                                ),
  CONFIG_VALUE_STRING            ("bd-blank-command",                 &globalOptions.bd.blankCommand,-1                              ),
  CONFIG_VALUE_STRING            ("bd-write-pre-command",             &globalOptions.bd.writePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("bd-write-post-command",            &globalOptions.bd.writePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("bd-write-command",                 &globalOptions.bd.writeCommand,-1                              ),
  CONFIG_VALUE_STRING            ("bd-write-image-command",           &globalOptions.bd.writeImageCommand,-1                         ),

//  CONFIG_VALUE_INTEGER64         ("file-max-storage-size",            &defaultFileServer.maxStorageSize,-1,                        0LL,MAX_INT64,NULL),
  CONFIG_VALUE_BEGIN_SECTION     ("file-server",-1),
    CONFIG_STRUCT_VALUE_INTEGER64("file-max-storage-size",            Server,maxStorageSize,                                         0LL,MAX_INT64,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("file-write-pre-command",           Server,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("file-write-post-command",          Server,writePostProcessCommand                                 ),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_STRING            ("ftp-login-name",                   &defaultFTPServer.ftp.loginName,-1                             ),
  CONFIG_VALUE_SPECIAL           ("ftp-password",                     &defaultFTPServer.ftp.password,-1,                             configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_INTEGER           ("ftp-max-connections",              &defaultFTPServer.maxConnectionCount,-1,                       0,MAX_INT,NULL),
  CONFIG_VALUE_INTEGER64         ("ftp-max-storage-size",             &defaultFTPServer.maxStorageSize,-1,                           0LL,MAX_INT64,NULL),
  CONFIG_VALUE_BEGIN_SECTION     ("ftp-server",-1),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-login-name",                   Server,ftp.loginName                                           ),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ftp-password",                     Server,ftp.password,                                           configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("ftp-max-connections",              Server,maxConnectionCount,                                     0,MAX_INT,NULL),
    CONFIG_STRUCT_VALUE_INTEGER64("ftp-max-storage-size",             Server,maxStorageSize,                                         0LL,MAX_INT64,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-write-pre-command",            Server,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-write-post-command",           Server,writePostProcessCommand                                 ),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_INTEGER           ("ssh-port",                         &defaultSSHServer.ssh.port,-1,                                 0,65535,NULL),
  CONFIG_VALUE_STRING            ("ssh-login-name",                   &defaultSSHServer.ssh.loginName,-1                             ),
  CONFIG_VALUE_SPECIAL           ("ssh-password",                     &defaultSSHServer.ssh.password,-1,                             configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_SPECIAL           ("ssh-public-key",                   &defaultSSHServer.ssh.publicKey,-1,                            configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("ssh-private-key",                  &defaultSSHServer.ssh.privateKey,-1,                           configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_INTEGER           ("ssh-max-connections",              &defaultSSHServer.maxConnectionCount,-1,                       0,MAX_INT,NULL),
  CONFIG_VALUE_INTEGER64         ("ssh-max-storage-size",             &defaultSSHServer.maxStorageSize,-1,                           0LL,MAX_INT64,NULL),
  CONFIG_VALUE_BEGIN_SECTION     ("ssh-server",-1),
    CONFIG_STRUCT_VALUE_INTEGER  ("ssh-port",                         Server,ssh.port,                                               0,65535,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-login-name",                   Server,ssh.loginName                                           ),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-password",                     Server,ssh.password,                                           configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-public-key",                   Server,ssh.publicKey,                                          configValueParseKeyData,configValueFormatInitKeyData,configValueFormatDoneKeyData,configValueFormatKeyData,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-private-key",                  Server,ssh.privateKey,                                         configValueParseKeyData,configValueFormatInitKeyData,configValueFormatDoneKeyData,configValueFormatKeyData,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("ssh-max-connections",              Server,maxConnectionCount,                                     0,MAX_INT,NULL),
    CONFIG_STRUCT_VALUE_INTEGER64("ssh-max-storage-size",             Server,maxStorageSize,                                         0LL,MAX_INT64,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-write-pre-command",            Server,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-write-post-command",           Server,writePostProcessCommand                                 ),
  CONFIG_VALUE_END_SECTION(),

//  CONFIG_VALUE_INTEGER           ("webdav-port",                      &defaultWebDAVServer.webDAV.port,-1,                           0,65535,NULL,NULL),
  CONFIG_VALUE_STRING            ("webdav-login-name",                &defaultWebDAVServer.webDAV.loginName,-1                       ),
  CONFIG_VALUE_SPECIAL           ("webdav-password",                  &defaultWebDAVServer.webDAV.password,-1,                       configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_INTEGER           ("webdav-max-connections",           &defaultWebDAVServer.maxConnectionCount,-1,                    0,MAX_INT,NULL),
  CONFIG_VALUE_INTEGER64         ("webdav-max-storage-size",          &defaultWebDAVServer.maxStorageSize,-1,                        0LL,MAX_INT64,NULL),
  CONFIG_VALUE_BEGIN_SECTION     ("webdav-server",-1),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-login-name",                Server,webDAV.loginName                                        ),
    CONFIG_STRUCT_VALUE_SPECIAL  ("webdav-password",                  Server,webDAV.password,                                        configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("webdav-max-connections",           Server,maxConnectionCount,                                     0,MAX_INT,NULL),
    CONFIG_STRUCT_VALUE_INTEGER64("webdav-max-storage-size",          Server,maxStorageSize,                                         0LL,MAX_INT64,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-write-pre-command",         Server,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-write-post-command",        Server,writePostProcessCommand                                 ),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_STRING            ("device",                           &defaultDevice.name,-1                                         ),
  CONFIG_VALUE_STRING            ("device-request-volume-command",    &defaultDevice.requestVolumeCommand,-1                         ),
  CONFIG_VALUE_STRING            ("device-unload-volume-command",     &defaultDevice.unloadVolumeCommand,-1                          ),
  CONFIG_VALUE_STRING            ("device-load-volume-command",       &defaultDevice.loadVolumeCommand,-1                            ),
  CONFIG_VALUE_INTEGER64         ("device-volume-size",               &defaultDevice.volumeSize,-1,                                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING            ("device-image-pre-command",         &defaultDevice.imagePreProcessCommand,-1                       ),
  CONFIG_VALUE_STRING            ("device-image-post-command",        &defaultDevice.imagePostProcessCommand,-1                      ),
  CONFIG_VALUE_STRING            ("device-image-command",             &defaultDevice.imageCommand,-1                                 ),
  CONFIG_VALUE_STRING            ("device-ecc-pre-command",           &defaultDevice.eccPreProcessCommand,-1                         ),
  CONFIG_VALUE_STRING            ("device-ecc-post-command",          &defaultDevice.eccPostProcessCommand,-1                        ),
  CONFIG_VALUE_STRING            ("device-ecc-command",               &defaultDevice.eccCommand,-1                                   ),
  CONFIG_VALUE_STRING            ("device-blank-command",             &defaultDevice.blankCommand,-1                                 ),
  CONFIG_VALUE_STRING            ("device-write-pre-command",         &defaultDevice.writePreProcessCommand,-1                       ),
  CONFIG_VALUE_STRING            ("device-write-post-command",        &defaultDevice.writePostProcessCommand,-1                      ),
  CONFIG_VALUE_STRING            ("device-write-command",             &defaultDevice.writeCommand,-1                                 ),
  CONFIG_VALUE_SECTION_ARRAY("device",-1,
    CONFIG_STRUCT_VALUE_STRING   ("device-name",                      Device,name                                                    ),
    CONFIG_STRUCT_VALUE_STRING   ("device-request-volume-command",    Device,requestVolumeCommand                                    ),
    CONFIG_STRUCT_VALUE_STRING   ("device-unload-volume-command",     Device,unloadVolumeCommand                                     ),
    CONFIG_STRUCT_VALUE_STRING   ("device-load-volume-command",       Device,loadVolumeCommand                                       ),
    CONFIG_STRUCT_VALUE_INTEGER64("device-volume-size",               Device,volumeSize,                                             0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-pre-command",         Device,imagePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-post-command",        Device,imagePostProcessCommand                                 ),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-command",             Device,imageCommand                                            ),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-pre-command",           Device,eccPreProcessCommand                                    ),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-post-command",          Device,eccPostProcessCommand                                   ),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-command",               Device,eccCommand                                              ),
    CONFIG_STRUCT_VALUE_STRING   ("device-blank-command",             Device,blankCommand                                            ),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-pre-command",         Device,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-post-command",        Device,writePostProcessCommand                                 ),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-command",             Device,writeCommand                                            ),
  ),

  // server settings
  CONFIG_VALUE_SELECT            ("server-mode",                      &serverMode,-1,                                                CONFIG_VALUE_SERVER_MODES),
  CONFIG_VALUE_INTEGER           ("server-port",                      &serverPort,-1,                                                0,65535,NULL),
  CONFIG_VALUE_INTEGER           ("server-tls-port",                  &serverTLSPort,-1,                                             0,65535,NULL),
  CONFIG_VALUE_SPECIAL           ("server-ca-file",                   &serverCA,-1,                                                  configValueParseCertificate,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("server-cert-file",                 &serverCert,-1,                                                configValueParseCertificate,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("server-key-file",                  &serverKey,-1,                                                 configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("server-password",                  &serverPasswordHash,-1,                                        configValueParseHashData,configValueFormatInitHashData,configValueFormatDoneHashData,configValueFormatHashData,NULL),
  CONFIG_VALUE_INTEGER           ("server-max-connections",           &serverMaxConnections,-1,                                      0,65535,NULL),

  CONFIG_VALUE_STRING            ("remote-bar-executable",            &globalOptions.remoteBARExecutable,-1                          ),

  CONFIG_VALUE_SET               ("log",                              &logTypes,-1,                                                  CONFIG_VALUE_LOG_TYPES),
  CONFIG_VALUE_CSTRING           ("log-file",                         &logFileName,-1                                                ),
  CONFIG_VALUE_CSTRING           ("log-format",                       &logFormat,-1                                                  ),
  CONFIG_VALUE_CSTRING           ("log-post-command",                 &logPostCommand,-1                                             ),

  CONFIG_VALUE_COMMENT           ("process id file"),
  CONFIG_VALUE_CSTRING           ("pid-file",                         &pidFileName,-1                                                ),

  CONFIG_VALUE_COMMENT           ("pairing master enable file"),
  CONFIG_VALUE_CSTRING           ("pairing-master-file",              &globalOptions.masterInfo.pairingFileName,-1                   ),

  // deprecated
  CONFIG_VALUE_DEPRECATED        ("server-jobs-directory",            &globalOptions.jobsDirectory,-1,                               ConfigValue_parseDeprecatedString,NULL,"jobs-directory",TRUE),
  CONFIG_VALUE_DEPRECATED        ("remote-host-name",                 NULL,-1,                                                       NULL,NULL,"slave-host-name",TRUE),
  CONFIG_VALUE_DEPRECATED        ("remote-host-port",                 NULL,-1,                                                       NULL,NULL,"slave-host-port",TRUE),
  CONFIG_VALUE_DEPRECATED        ("remote-host-force-ssl",            NULL,-1,                                                       NULL,NULL,"slave-host-force-tls",TRUE),
  CONFIG_VALUE_DEPRECATED        ("slave-host-force-ssl",             NULL,-1,                                                       NULL,NULL,"slave-host-force-tls",TRUE),
  // Note: archive-file-mode=overwrite
  CONFIG_VALUE_DEPRECATED        ("overwrite-archive-files",          &globalOptions.archiveFileMode,-1,                             configValueParseDeprecatedArchiveFileModeOverwrite,NULL,"archive-file-mode",TRUE),
  // Note: restore-entry-mode=overwrite
  CONFIG_VALUE_DEPRECATED        ("overwrite-files",                  &globalOptions.restoreEntryMode,-1,                            configValueParseDeprecatedRestoreEntryModeOverwrite,NULL,"restore-entry-mode=overwrite",TRUE),
  CONFIG_VALUE_DEPRECATED        ("mount-device",                     &globalOptions.mountList,-1,                                   configValueParseDeprecatedMountDevice,NULL,"mount",TRUE),
  CONFIG_VALUE_DEPRECATED        ("schedule",                         NULL,-1,                                                       NULL,NULL,NULL,TRUE),
  CONFIG_VALUE_DEPRECATED        ("stop-on-error",                    &globalOptions.noStopOnErrorFlag,-1,                           configValueParseDeprecatedStopOnError,NULL,"no-stop-on-error",TRUE),

  // ignored
);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : openLog
* Purpose: open log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void openLog(void)
{
  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (logFileName != NULL)
    {
      logFile = fopen(logFileName,"a");
      if (logFile == NULL) printWarning("Cannot open log file '%s' (error: %s)!",logFileName,strerror(errno));
    }
  }
}

/***********************************************************************\
* Name   : closeLog
* Purpose: close log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void closeLog(void)
{
  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (logFile != NULL)
    {
      fclose(logFile);
      logFile = NULL;
    }
  }
}

/***********************************************************************\
* Name   : reopenLog
* Purpose: re-open log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void reopenLog(void)
{
  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (logFileName != NULL)
    {
      fclose(logFile);
      logFile = fopen(logFileName,"a");
      if (logFile == NULL) printWarning("Cannot re-open log file '%s' (error: %s)!",logFileName,strerror(errno));
    }
  }
}

/***********************************************************************\
* Name   : signalHandler
* Purpose: general signal handler
* Input  : signalNumber - signal number
*          siginfo      - signal info
*          context      - context
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SIGACTION
LOCAL void signalHandler(int signalNumber, siginfo_t *siginfo, void *context)
#else /* not HAVE_SIGACTION */
LOCAL void signalHandler(int signalNumber)
#endif /* HAVE_SIGACTION */
{
  #ifdef HAVE_SIGACTION
    struct sigaction signalAction;
  #endif /* HAVE_SIGACTION */

  #ifdef HAVE_SIGINFO_T
    UNUSED_VARIABLE(siginfo);
    UNUSED_VARIABLE(context);
  #endif /* HAVE_SIGINFO_T */

  // reopen log file
  #ifdef HAVE_SIGUSR1
    if (signalNumber == SIGUSR1)
    {
      reopenLog();
      return;
    }
  #endif /* HAVE_SIGUSR1 */

  // deinstall signal handlers
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_flags   = 0;
    signalAction.sa_handler = SIG_DFL;
    sigaction(SIGTERM,&signalAction,NULL);
    sigaction(SIGILL,&signalAction,NULL);
    sigaction(SIGFPE,&signalAction,NULL);
    sigaction(SIGSEGV,&signalAction,NULL);
    sigaction(SIGBUS,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGTERM,SIG_DFL);
    signal(SIGILL,SIG_DFL);
    signal(SIGFPE,SIG_DFL);
    signal(SIGSEGV,SIG_DFL);
    #ifdef HAVE_SIGBUS
      signal(SIGBUS,SIG_DFL);
    #endif /* HAVE_SIGBUS */
  #endif /* HAVE_SIGACTION */

  // output error message
  fprintf(stderr,"INTERNAL ERROR: signal %d\n",signalNumber);
  #ifndef NDEBUG
    debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,1);
  #endif /* NDEBUG */

  // delete pid file
  deletePIDFile();

  // delete temporary directory (Note: do a simple validity check in case something serious went wrong...)
  if (!String_isEmpty(tmpDirectory) && !String_equalsCString(tmpDirectory,"/"))
  {
    (void)File_delete(tmpDirectory,TRUE);
  }

  // Note: do not free resources to avoid further errors

  // exit with signal number
  exit(128+signalNumber);
}

/***********************************************************************\
* Name   : outputLineInit
* Purpose: init output line variable instance callback
* Input  : userData - user data (not used)
* Output : -
* Return : output line variable instance
* Notes  : -
\***********************************************************************/

LOCAL void *outputLineInit(void *userData)
{
  UNUSED_VARIABLE(userData);

  return String_new();
}

/***********************************************************************\
* Name   : outputLineDone
* Purpose: done output line variable instance callback
* Input  : variable - output line variable instance
*          userData - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputLineDone(void *variable, void *userData)
{
  assert(variable != NULL);

  UNUSED_VARIABLE(userData);

  String_delete((String)variable);
}

/***********************************************************************\
* Name   : outputConsole
* Purpose: output string to console
* Input  : file   - output stream (stdout, stderr)
*          string - string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputConsole(FILE *file, ConstString string)
{
  String outputLine;
  ulong  i;
  size_t bytesWritten;
  char   ch;

  assert(file != NULL);
  assert(Semaphore_isLocked(&consoleLock));

  outputLine = (String)Thread_getLocalVariable(&outputLineHandle);
  if (outputLine != NULL)
  {
    if (File_isTerminal(file))
    {
      // restore output line if different to current line
      if (outputLine != lastOutputLine)
      {
        // wipe-out last line
        if (lastOutputLine != NULL)
        {
          for (i = 0; i < String_length(lastOutputLine); i++)
          {
            bytesWritten = fwrite("\b",1,1,file);
          }
          for (i = 0; i < String_length(lastOutputLine); i++)
          {
            bytesWritten = fwrite(" ",1,1,file);
          }
          for (i = 0; i < String_length(lastOutputLine); i++)
          {
            bytesWritten = fwrite("\b",1,1,file);
          }
          fflush(file);
        }

        // restore line
        bytesWritten = fwrite(String_cString(outputLine),1,String_length(outputLine),file);
      }

      // output new string
      bytesWritten = fwrite(String_cString(string),1,String_length(string),file);

      // store output string
      STRING_CHAR_ITERATE(string,i,ch)
      {
        switch (ch)
        {
          case '\n':
            String_clear(outputLine);
            break;
          case '\b':
            String_remove(outputLine,STRING_END,1);
            break;
          default:
            String_appendChar(outputLine,ch);
            break;
        }
      }

      lastOutputLine = outputLine;
    }
    else
    {
      if (String_index(string,STRING_END) == '\n')
      {
        if (outputLine != NULL) bytesWritten = fwrite(String_cString(outputLine),1,String_length(outputLine),file);
        bytesWritten = fwrite(String_cString(string),1,String_length(string),file);
        String_clear(outputLine);
      }
      else
      {
        String_append(outputLine,string);
      }
    }
    fflush(file);
  }
  else
  {
    // no thread local vairable -> output string
    bytesWritten = fwrite(String_cString(string),1,String_length(string),file);
  }
  UNUSED_VARIABLE(bytesWritten);
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

  deviceNode->id          = Misc_getId();
  deviceNode->device.name = String_duplicate(name);

  return deviceNode;
}

/***********************************************************************\
* Name   : freeMountedNode
* Purpose: free mounted node
* Input  : mountedNode - mounted node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeMountedNode(MountedNode *mountedNode, void *userData)
{
  assert(mountedNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(mountedNode->device);
  String_delete(mountedNode->name);
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
  long       nextIndex;

  assert(fileName != NULL);

  // init print info
  printInfoFlag = isPrintInfo(2) || printInfoFlag;

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
  SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (printInfoFlag) { printConsole(stdout,0,"Reading configuration file '%s'...",String_cString(fileName)); }
    while (   !failFlag
           && File_getLine(&fileHandle,line,&lineNb,"#")
          )
    {
      // parse line
      if      (String_parse(line,STRING_BEGIN,"[file-server %S]",NULL,name))
      {
        ServerNode *serverNode;

        // find/allocate server node
        serverNode = NULL;
        SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,
                                              serverNode,
                                                 (serverNode->server.type == SERVER_TYPE_FILE)
                                              && String_equals(serverNode->server.name,name)
                                             );
          if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
        }
        if (serverNode == NULL) serverNode = newServerNode(name,SERVER_TYPE_FILE);
        assert(serverNode != NULL);
        assert(serverNode->server.type == SERVER_TYPE_FILE);

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
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
                                    &serverNode->server
                                   );
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
                                                 (serverNode->server.type == SERVER_TYPE_FTP)
//TODO: port number
                                              && String_equals(serverNode->server.name,name)
                                             );
          if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
        }
        if (serverNode == NULL) serverNode = newServerNode(name,SERVER_TYPE_FTP);
        assert(serverNode != NULL);
        assert(serverNode->server.type == SERVER_TYPE_FTP);

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
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
                                    &serverNode->server
                                   );
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
                                                 (serverNode->server.type == SERVER_TYPE_SSH)
//TODO: port number
                                              && String_equals(serverNode->server.name,name)
                                             );
          if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
        }
        if (serverNode == NULL) serverNode = newServerNode(name,SERVER_TYPE_SSH);
        assert(serverNode != NULL);
        assert(serverNode->server.type == SERVER_TYPE_SSH);

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
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
                                    &serverNode->server
                                   );
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
                                                 (serverNode->server.type == SERVER_TYPE_WEBDAV)
//TODO: port number
                                              && String_equals(serverNode->server.name,name)
                                             );
          if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
        }
        if (serverNode == NULL) serverNode = newServerNode(name,SERVER_TYPE_WEBDAV);
        assert(serverNode != NULL);
        assert(serverNode->server.type == SERVER_TYPE_WEBDAV);

        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
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
                                    &serverNode->server
                                   );
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
        while (   File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
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
                                    &deviceNode->device
                                   );
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
        // parse section
        while (   !failFlag
               && File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
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
                                    &globalOptions.masterInfo
                                   );
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
                                NULL  // variable
                               );
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

  // free resources
  String_delete(value);
  String_delete(name);
  String_delete(line);

  // close file
  (void)File_close(&fileHandle);

  return !failFlag;
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
* Name   : readCertificateFile
* Purpose: read certificate file
* Input  : certificate - certificate variable
*          fileName    - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readCertificateFile(Certificate *certificate, const char *fileName)
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

/***********************************************************************\
* Name   : readKeyFile
* Purpose: read public/private key file
* Input  : key      - key variable
*          fileName - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readKeyFile(Key *key, const char *fileName)
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
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  StringList_appendCString(&configFileNameList,value);

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
  switch (command)
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
      HALT_INTERNAL_ERROR("no valid command set (%d)",command);
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

  // allocate new schedule node
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
      if (!parseDateTimeNumber(s0,&bandWidthNode->year ))
      {
        if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth year '%s'",String_cString(s0));
        errorFlag = TRUE;
      }
      if (!parseDateMonth     (s1,&bandWidthNode->month))
      {
        if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth month '%s'",String_cString(s1));
        errorFlag = TRUE;
      }
      if (!parseDateTimeNumber(s2,&bandWidthNode->day  ))
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
        if (!parseWeekDaySet(String_cString(s0),&bandWidthNode->weekDaySet))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth weekday '%s'",String_cString(s0));
          errorFlag = TRUE;
        }
        if (!parseDateTimeNumber(s1,&bandWidthNode->hour  ))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth hour '%s'",String_cString(s1));
          errorFlag = TRUE;
        }
        if (!parseDateTimeNumber(s2,&bandWidthNode->minute))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth minute '%s'",String_cString(s2));
          errorFlag = TRUE;
        }
      }
      else if (String_parse(s,nextIndex,"%S:%S",&nextIndex,s0,s1))
      {
        if (!parseDateTimeNumber(s0,&bandWidthNode->hour  ))
        {
          if (!errorFlag) stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth hour '%s'",String_cString(s0));
          errorFlag = TRUE;
        }
        if (!parseDateTimeNumber(s1,&bandWidthNode->minute))
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
    String_delete(bandWidthNode->fileName);
    LIST_DELETE_NODE(bandWidthNode);
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
    n = strlen(user);
    if      ((n >= 1) && (toupper(user[0]) == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1]) == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2]) == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
    n = strlen(group);
    if      ((n >= 1) && (toupper(group[0]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
    else if ((n >= 2) && (toupper(group[1]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
    else if ((n >= 3) && (toupper(group[2]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
    n = strlen(world);
    if      ((n >= 1) && (toupper(world[0]) == 'R')) permission |= FILE_PERMISSION_OTHER_READ;
    else if ((n >= 2) && (toupper(world[1]) == 'W')) permission |= FILE_PERMISSION_OTHER_WRITE;
    else if ((n >= 3) && (toupper(world[2]) == 'X')) permission |= FILE_PERMISSION_OTHER_EXECUTE;
  }
  else if (String_scanCString(value,"%4s:%4s",user,group))
  {
    n = strlen(user);
    if      ((n >= 1) && (toupper(user[0]) == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1]) == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2]) == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
    n = strlen(group);
    if      ((n >= 1) && (toupper(group[0]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
    else if ((n >= 2) && (toupper(group[1]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
    else if ((n >= 3) && (toupper(group[2]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
  }
  else if (String_scanCString(value,"%4s",user))
  {
    n = strlen(user);
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
  setHash((Hash*)variable,&cryptHash);

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

  error = readCertificateFile(certificate,value);
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

  error = readKeyFile(key,value);
  if (error != ERROR_NONE)
  {
    stringSet(errorMessage,errorMessageSize,Error_getText(error));
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseKeyData
* Purpose: command line option call back for get key data
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseKeyData(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
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
    error = readKeyFile(key,value);
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,errorMessageSize,Error_getText(error));
      return FALSE;
    }
  }
  else if (stringStartsWith(value,"base64:"))
  {
    // base64-prefixed key string

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
    dataLength = strlen(value);
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
      memcpy(data,value,dataLength);

      // set key data
      if (key->data != NULL) freeSecure(key->data);
      key->data   = data;
      key->length = dataLength;
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParsePublicPrivateKey
* Purpose: command line option call back for get public/private key
*          (without password and salt)
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParsePublicPrivateKey(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  CryptKey *cryptKey = (CryptKey*)variable;
  Errors   error;
  String   fileName;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  // get key data
  if (File_existsCString(value))
  {
    // key file base64 encoded

    // read key file
    fileName = String_newCString(value);
    error = Crypt_readPublicPrivateKeyFile(cryptKey,
                                           fileName,
                                           CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                           CRYPT_KEY_DERIVE_NONE,
                                           NULL,  // cryptSalt
                                           NULL  // password
                                          );
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,errorMessageSize,Error_getText(error));
      String_delete(fileName);
      return FALSE;
    }
    String_delete(fileName);
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
    // plain key string

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
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String string;

  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(variable);
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

  // append to config filename list
  StringList_append(&configFileNameList,string);

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedMountDevice
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

LOCAL bool configValueParseDeprecatedMountDevice(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
* Name   : configValueParseDeprecatedStopOnError
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

LOCAL bool configValueParseDeprecatedStopOnError(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
* Name   : configValueParseDeprecatedArchiveFileModeOverwrite
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

LOCAL bool configValueParseDeprecatedArchiveFileModeOverwrite(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
* Name   : configValueParseDeprecatedRestoreEntryModeOverwrite
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

LOCAL bool configValueParseDeprecatedRestoreEntryModeOverwrite(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
* Name       : printUsage
* Purpose    : print "usage" help
* Input      : programName - program name
*              level       - help level (0..1)
* Output     : -
* Return     : -
* Side-effect: unknown
* Notes      : -
\***********************************************************************/

LOCAL void printUsage(const char *programName, uint level)
{
  assert(programName != NULL);
  printf("Usage: %s [<options>] [--] <archive name> [<files>|<device>...]\n",programName);
  printf("       %s [<options>] --generate-keys|--generate-signature-keys [--] [<key file base name>]\n",programName);
  printf("\n");
  printf("Archive name:  <file name>\n");
  printf("               file://<file name>\n");
  printf("               ftp://[<login name>[:<password>]@]<host name>/<file name>\n");
  printf("               scp://[<login name>[:<password>]@]<host name>[:<port>]/<file name>\n");
  printf("               sftp://[<login name>[:<password>]@]<host name>[:<port>]/<file name>\n");
  printf("               webdav://[<login name>[:<password>]@]<host name>/<file name>\n");
  printf("               webdavs://[<login name>[:<password>]@]<host name>/<file name>\n");
  printf("               cd://[<device name>:]<file name>\n");
  printf("               dvd://[<device name>:]<file name>\n");
  printf("               bd://[<device name>:]<file name>\n");
  printf("               device://[<device name>:]<file name>\n");
  printf("\n");
  CmdOption_printHelp(stdout,
                      COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                      level
                     );
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
* Name   : initGlobalOptions
* Purpose: initialize global option values
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initGlobalOptions(void)
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
  initKey(&globalOptions.masterInfo.publicKey);
  List_init(&globalOptions.maxBandWidthList);
  globalOptions.maxBandWidthList.n                              = 0L;
  globalOptions.maxBandWidthList.lastReadTimestamp              = 0LL;

  List_init(&globalOptions.serverList);
  Semaphore_init(&globalOptions.serverList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&globalOptions.deviceList);
  Semaphore_init(&globalOptions.deviceList.lock,SEMAPHORE_TYPE_BINARY);

  globalOptions.indexDatabaseAutoUpdateFlag                     = TRUE;
  List_init(&globalOptions.indexDatabaseMaxBandWidthList);
  globalOptions.indexDatabaseMaxBandWidthList.n                 = 0L;
  globalOptions.indexDatabaseMaxBandWidthList.lastReadTimestamp = 0LL;
  globalOptions.indexDatabaseKeepTime                           = S_PER_DAY;

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

  globalOptions.quietFlag                                       = FALSE;
  globalOptions.verboseLevel                                    = DEFAULT_VERBOSE_LEVEL;

  globalOptions.serverDebugLevel                                = DEFAULT_SERVER_DEBUG_LEVEL;
  globalOptions.serverDebugIndexOperationsFlag                  = FALSE;

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
  initKey(&globalOptions.cryptPublicKey);
  initKey(&globalOptions.cryptPrivateKey);
  Crypt_initKey(&globalOptions.signaturePublicKey,CRYPT_PADDING_TYPE_NONE);
  Crypt_initKey(&globalOptions.signaturePrivateKey,CRYPT_PADDING_TYPE_NONE);

  globalOptions.fileServer                                      = &defaultFileServer;
  globalOptions.defaultFileServer                               = &defaultFileServer;
  globalOptions.ftpServer                                       = &defaultFTPServer;
  globalOptions.defaultFTPServer                                = &defaultFTPServer;
  globalOptions.sshServer                                       = &defaultSSHServer;
  globalOptions.defaultSSHServer                                = &defaultSSHServer;
  globalOptions.webDAVServer                                    = &defaultWebDAVServer;
  globalOptions.defaultWebDAVServer                             = &defaultWebDAVServer;

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

  globalOptions.defaultDevice                                   = &defaultDevice;
  globalOptions.device                                          = globalOptions.defaultDevice;

  globalOptions.archiveFileMode                                 = ARCHIVE_FILE_MODE_STOP;
  globalOptions.restoreEntryMode                                = RESTORE_ENTRY_MODE_STOP;
  globalOptions.skipUnreadableFlag                              = TRUE;
  globalOptions.errorCorrectionCodesFlag                        = FALSE;
  globalOptions.waitFirstVolumeFlag                             = FALSE;

  VALUESET_CLEAR(globalOptionSet);
}

/***********************************************************************\
* Name   : doneGlobalOptions
* Purpose: deinitialize global option values
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneGlobalOptions(void)
{
  List_done(&globalOptions.indexDatabaseMaxBandWidthList,CALLBACK_((ListNodeFreeFunction)freeBandWidthNode,NULL));
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

  List_done(&globalOptions.deviceList,CALLBACK_((ListNodeFreeFunction)freeDeviceNode,NULL));
  Semaphore_done(&globalOptions.deviceList.lock);
  List_done(&globalOptions.serverList,CALLBACK_((ListNodeFreeFunction)freeServerNode,NULL));
  Semaphore_done(&globalOptions.serverList.lock);

  Crypt_doneKey(&globalOptions.signaturePrivateKey);
  Crypt_doneKey(&globalOptions.signaturePublicKey);
  doneKey(&globalOptions.cryptPrivateKey);
  doneKey(&globalOptions.cryptPublicKey);
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

  List_done(&globalOptions.maxBandWidthList,CALLBACK_((ListNodeFreeFunction)freeBandWidthNode,NULL));
  doneKey(&globalOptions.masterInfo.publicKey);
  doneHash(&globalOptions.masterInfo.uuidHash);
  String_delete(globalOptions.masterInfo.name);
  String_delete(globalOptions.incrementalDataDirectory);
  String_delete(globalOptions.jobsDirectory);
  String_delete(globalOptions.tmpDirectory);
  String_delete(globalOptions.barExecutable);
}

/***********************************************************************\
* Name   : initAll
* Purpose: initialize
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initAll(void)
{
  #ifdef HAVE_SIGACTION
    struct sigaction signalAction;
  #endif /* HAVE_SIGACTION */
  AutoFreeList     autoFreeList;
  Errors           error;
  #if defined(HAVE_SETLOCALE) && defined(HAVE_BINDTEXTDOMAIN) && defined(HAVE_TEXTDOMAIN)
    const char       *localePath;
  #endif /* defined(HAVE_SETLOCALE) && defined(HAVE_BINDTEXTDOMAIN) && defined(HAVE_TEXTDOMAIN) */

  // initialize fatal log handler, crash dump handler
  #ifndef NDEBUG
    debugDumpStackTraceAddOutput(DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_FATAL,fatalLogMessage,NULL);
  #endif /* not NDEBUG */
  #if HAVE_BREAKPAD
    if (!MiniDump_init())
    {
      (void)fprintf(stderr,"Warning: Cannot initialize crash dump handler. No crash dumps will be created.\n");
    }
  #endif /* HAVE_BREAKPAD */

  // install signal handlers
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_flags     = SA_SIGINFO;
    signalAction.sa_sigaction = signalHandler;
    sigaction(SIGSEGV,&signalAction,NULL);
    sigaction(SIGFPE,&signalAction,NULL);
    sigaction(SIGILL,&signalAction,NULL);
    sigaction(SIGTERM,&signalAction,NULL);
    sigaction(SIGUSR1,&signalAction,NULL);
    sigaction(SIGBUS,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGSEGV,signalHandler);
    signal(SIGTERM,signalHandler);
    signal(SIGILL,signalHandler);
    signal(SIGFPE,signalHandler);
    signal(SIGSEGV,signalHandler);
    #ifdef HAVE_SIGBUS
      signal(SIGBUS,signalHandler);
    #endif /* HAVE_SIGBUS */
  #endif /* HAVE_SIGACTION */

  AutoFree_init(&autoFreeList);

  // init secure memory
  error = initSecure();
  if (error != ERROR_NONE)
  {
    return error;
  }
  DEBUG_TESTCODE() { doneSecure(); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,initSecure,{ doneSecure(); });

  // initialize variables
  Semaphore_init(&consoleLock,SEMAPHORE_TYPE_BINARY);
  DEBUG_TESTCODE() { Semaphore_done(&consoleLock); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  initGlobalOptions();
  uuid                                   = String_new();

  tmpDirectory                           = String_new();

  #ifdef HAVE_NEWLOCALE
    POSIXLocale                          = newlocale(LC_ALL,"POSIX",0);
  #endif /* HAVE_NEWLOCALE */

  Semaphore_init(&mountedList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&mountedList);

  command                                = COMMAND_NONE;
  jobUUIDName                            = String_new();

  changeToDirectory                      = NULL;

  jobUUID                                = String_new();
  storageName                            = String_new();
  EntryList_init(&includeEntryList);
  PatternList_init(&excludePatternList);
  initServer(&defaultFileServer,NULL,SERVER_TYPE_FILE);
  initServer(&defaultFTPServer,NULL,SERVER_TYPE_FTP);
  initServer(&defaultSSHServer,NULL,SERVER_TYPE_SSH);
  initServer(&defaultWebDAVServer,NULL,SERVER_TYPE_WEBDAV);
  initDevice(&defaultDevice);
  serverMode                             = SERVER_MODE_MASTER;
  serverPort                             = DEFAULT_SERVER_PORT;
  serverTLSPort                          = DEFAULT_TLS_SERVER_PORT;
  initCertificate(&serverCA);
  initCertificate(&serverCert);
  initKey(&serverKey);
  initHash(&serverPasswordHash);
  serverMaxConnections                   = DEFAULT_MAX_SERVER_CONNECTIONS;

  continuousDatabaseFileName             = NULL;
  indexDatabaseFileName                  = NULL;

  logTypes                               = LOG_TYPE_NONE;
  logFileName                            = NULL;
  logFormat                              = DEFAULT_LOG_FORMAT;
  logPostCommand                         = NULL;

  batchFlag                              = FALSE;

  pidFileName                            = NULL;

  generateKeyBits                        = MIN_ASYMMETRIC_CRYPT_KEY_BITS;
  generateKeyMode                        = CRYPT_KEY_MODE_NONE;

  StringList_init(&configFileNameList);
  configModified                         = FALSE;

  Semaphore_init(&logLock,SEMAPHORE_TYPE_BINARY);
  logFile                                = NULL;

  Thread_initLocalVariable(&outputLineHandle,outputLineInit,NULL);
  lastOutputLine                         = NULL;

  AUTOFREE_ADD(&autoFreeList,&consoleLock,{ Semaphore_done(&consoleLock); });
  AUTOFREE_ADD(&autoFreeList,&globalOptions,{ doneGlobalOptions(); });
  AUTOFREE_ADD(&autoFreeList,uuid,{ String_delete(uuid); });
  AUTOFREE_ADD(&autoFreeList,tmpDirectory,{ String_delete(tmpDirectory); });
  AUTOFREE_ADD(&autoFreeList,&mountedList,{ List_done(&mountedList,CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL)); });
  AUTOFREE_ADD(&autoFreeList,&mountedList.lock,{ Semaphore_done(&mountedList.lock); });
  AUTOFREE_ADD(&autoFreeList,jobUUIDName,{ String_delete(jobUUIDName); });
  AUTOFREE_ADD(&autoFreeList,jobUUID,{ String_delete(jobUUID); });
  AUTOFREE_ADD(&autoFreeList,storageName,{ String_delete(storageName); });
  AUTOFREE_ADD(&autoFreeList,&includeEntryList,{ EntryList_done(&includeEntryList); });
  AUTOFREE_ADD(&autoFreeList,&excludePatternList,{ PatternList_done(&excludePatternList); });
  AUTOFREE_ADD(&autoFreeList,&defaultFileServer,{ doneServer(&defaultFileServer); });
  AUTOFREE_ADD(&autoFreeList,&defaultFTPServer,{ doneServer(&defaultFTPServer); });
  AUTOFREE_ADD(&autoFreeList,&defaultSSHServer,{ doneServer(&defaultSSHServer); });
  AUTOFREE_ADD(&autoFreeList,&defaultWebDAVServer,{ doneServer(&defaultWebDAVServer); });
  AUTOFREE_ADD(&autoFreeList,&defaultDevice,{ doneDevice(&defaultDevice); });
  AUTOFREE_ADD(&autoFreeList,&serverCert,{ doneCertificate(&serverCert); });
  AUTOFREE_ADD(&autoFreeList,&serverKey,{ doneKey(&serverKey); });
  AUTOFREE_ADD(&autoFreeList,&serverPasswordHash,{ doneHash(&serverPasswordHash); });
  AUTOFREE_ADD(&autoFreeList,&configFileNameList,{ StringList_done(&configFileNameList); });
  AUTOFREE_ADD(&autoFreeList,&logLock,{ Semaphore_done(&logLock); });
  AUTOFREE_ADD(&autoFreeList,&outputLineHandle,{ Thread_doneLocalVariable(&outputLineHandle,outputLineDone,NULL); });

  // initialize i18n
  #if defined(HAVE_SETLOCALE) && defined(HAVE_BINDTEXTDOMAIN) && defined(HAVE_TEXTDOMAIN)
    setlocale(LC_ALL,"");
    #ifdef HAVE_BINDTEXTDOMAIN
      localePath = getenv("__BAR_LOCALE__");
      if (localePath != NULL)
      {
        bindtextdomain("bar",localePath);
      }
    #endif /* HAVE_BINDTEXTDOMAIN */
    textdomain("bar");
  #endif /* HAVE_SETLOCAL && HAVE_TEXTDOMAIN */

  // initialize modules
  error = Thread_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Thread_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Thread_initAll,{ Thread_doneAll(); });

  error = Password_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Password_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Password_initAll,{ Password_doneAll(); });

  error = Compress_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Compress_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Compress_initAll,{ Compress_doneAll(); });

  error = Crypt_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Crypt_initAll,{ Crypt_doneAll(); });

  error = EntryList_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { EntryList_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,EntryList_initAll,{ EntryList_doneAll(); });

  error = Pattern_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Pattern_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Pattern_initAll,{ Password_doneAll(); });

  error = PatternList_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { PatternList_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,PatternList_initAll,{ PatternList_doneAll(); });

  error = Chunk_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Chunk_initAll,{ Chunk_doneAll(); });

  error = DeltaSource_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { DeltaSource_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,DeltaSource_initAll,{ DeltaSource_doneAll(); });

  error = Archive_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Archive_initAll,{ Archive_doneAll(); });

  error = Storage_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Storage_doneAll(), AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Storage_initAll,{ Storage_doneAll(); });

  error = Index_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Index_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Index_initAll,{ Index_doneAll(); });

  (void)Continuous_initAll();
  DEBUG_TESTCODE() { Continuous_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Continuous_initAll,{ Continuous_doneAll(); });

  error = Network_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Network_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Network_initAll,{ Network_doneAll(); });

  error = Job_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Job_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Job_initAll,{ Job_doneAll(); });

  error = Server_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Server_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Server_initAll,{ Server_doneAll(); });

  // initialize command line options and config values
  ConfigValue_init(CONFIG_VALUES);
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));

  // special case: set verbose level in interactive mode
  if (!daemonFlag && !batchFlag)
  {
    globalOptions.verboseLevel = DEFAULT_VERBOSE_LEVEL_INTERACTIVE;
  }

  // done resources
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : doneAll
* Purpose: deinitialize
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneAll(void)
{
  #ifdef HAVE_SIGACTION
    struct sigaction signalAction;
  #endif /* HAVE_SIGACTION */

  // deinitialize modules
  Server_doneAll();
  Job_doneAll();
  Network_doneAll();
  Continuous_doneAll();
  Index_doneAll();
  Storage_doneAll();
  Archive_doneAll();
  DeltaSource_doneAll();
  Chunk_doneAll();
  PatternList_doneAll();
  Pattern_doneAll();
  EntryList_doneAll();
  Crypt_doneAll();
  Compress_doneAll();
  Password_doneAll();
  Thread_doneAll();

  // deinitialize command line options and config values
  CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  ConfigValue_done(CONFIG_VALUES);

  // done server ca, cert, key
  doneKey(&serverKey);
  doneCertificate(&serverCert);
  doneCertificate(&serverCA);

  // deinitialize variables
  #ifdef HAVE_NEWLOCALE
    freelocale(POSIXLocale);
  #endif /* HAVE_NEWLOCALE */
  Semaphore_done(&logLock);
  if (defaultDevice.writeCommand != NULL) String_delete(defaultDevice.writeCommand);
  if (defaultDevice.writePostProcessCommand != NULL) String_delete(defaultDevice.writePostProcessCommand);
  if (defaultDevice.writePreProcessCommand != NULL) String_delete(defaultDevice.writePreProcessCommand);
  if (defaultDevice.eccCommand != NULL) String_delete(defaultDevice.eccCommand);
  if (defaultDevice.blankCommand != NULL) String_delete(defaultDevice.blankCommand);
  if (defaultDevice.eccPostProcessCommand != NULL) String_delete(defaultDevice.eccPostProcessCommand);
  if (defaultDevice.eccPreProcessCommand != NULL) String_delete(defaultDevice.eccPreProcessCommand);
  if (defaultDevice.imageCommand != NULL) String_delete(defaultDevice.imageCommand);
  if (defaultDevice.imagePostProcessCommand != NULL) String_delete(defaultDevice.imagePostProcessCommand);
  if (defaultDevice.imagePreProcessCommand != NULL) String_delete(defaultDevice.imagePreProcessCommand);
  if (defaultDevice.unloadVolumeCommand != NULL) String_delete(defaultDevice.unloadVolumeCommand);
  if (defaultDevice.loadVolumeCommand != NULL) String_delete(defaultDevice.loadVolumeCommand);
  if (defaultDevice.requestVolumeCommand != NULL) String_delete(defaultDevice.requestVolumeCommand);
  if (defaultDevice.name != NULL) String_delete(defaultDevice.name);

  Thread_doneLocalVariable(&outputLineHandle,outputLineDone,NULL);
  StringList_done(&configFileNameList);
  doneHash(&serverPasswordHash);
  doneServer(&defaultWebDAVServer);
  doneServer(&defaultSSHServer);
  doneServer(&defaultFTPServer);
  doneServer(&defaultFileServer);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  String_delete(storageName);
  String_delete(jobUUID);

  String_delete(jobUUIDName);

  List_done(&mountedList,CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL));
  Semaphore_done(&mountedList.lock);

  String_delete(tmpDirectory);
  String_delete(uuid);
  doneGlobalOptions();

  Semaphore_done(&consoleLock);

  // done secure memory
  doneSecure();

  // deinstall signal handlers
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_flags   = 0;
    signalAction.sa_handler = SIG_DFL;
    sigaction(SIGUSR1,&signalAction,NULL);
    sigaction(SIGTERM,&signalAction,NULL);
    sigaction(SIGILL,&signalAction,NULL);
    sigaction(SIGFPE,&signalAction,NULL);
    sigaction(SIGSEGV,&signalAction,NULL);
    sigaction(SIGBUS,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGTERM,SIG_DFL);
    signal(SIGILL,SIG_DFL);
    signal(SIGFPE,SIG_DFL);
    signal(SIGSEGV,SIG_DFL);
    #ifdef HAVE_SIGBUS
      signal(SIGBUS,SIG_DFL);
    #endif /* HAVE_SIGBUS */
  #endif /* HAVE_SIGACTION */

  // deinitialize crash dump handler
  #if HAVE_BREAKPAD
    MiniDump_done();
  #endif /* HAVE_BREAKPAD */
}

/***********************************************************************\
* Name   : readAllServerKeys
* Purpose: initialize all server keys/certificates
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readAllServerKeys(void)
{
  String fileName;
  Key    key;

  // init default servers
  fileName = String_new();
  initKey(&key);
  File_appendFileNameCString(String_setCString(fileName,getenv("HOME")),".ssh/id_rsa.pub");
  if (File_exists(fileName) && (readKeyFile(&key,String_cString(fileName)) == ERROR_NONE))
  {
    duplicateKey(&defaultSSHServer.ssh.publicKey,&key);
    duplicateKey(&defaultWebDAVServer.webDAV.publicKey,&key);
  }
  File_appendFileNameCString(String_setCString(fileName,getenv("HOME")),".ssh/id_rsa");
  if (File_exists(fileName) && (readKeyFile(&key,String_cString(fileName)) == ERROR_NONE))
  {
    duplicateKey(&defaultSSHServer.ssh.privateKey,&key);
    duplicateKey(&defaultWebDAVServer.webDAV.privateKey,&key);
  }
  doneKey(&key);
  String_delete(fileName);

  // read default server CA, certificate, key
  (void)readCertificateFile(&serverCA,DEFAULT_TLS_SERVER_CA_FILE);
  (void)readCertificateFile(&serverCert,DEFAULT_TLS_SERVER_CERTIFICATE_FILE);
  (void)readKeyFile(&serverKey,DEFAULT_TLS_SERVER_KEY_FILE);

  return ERROR_NONE;
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
  StringNode *stringNode;

  assert(fileName != NULL);

  String_clear(fileName);

  stringNode = STRINGLIST_FIND_LAST(&configFileNameList,configFileName,File_isWritable(configFileName));
  if (stringNode != NULL)
  {
    String_set(fileName,stringNode->string);
  }

  return fileName;
}

Errors updateConfig(void)
{
  String            configFileName;
  StringList        configLinesList;
  String            line;
  Errors            error;
  int               i;
  StringNode        *nextStringNode;
  ConfigValueFormat configValueFormat;
  ServerNode        *serverNode;

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

  // update storage servers
  nextStringNode = ConfigValue_deleteSections(&configLinesList,"file-server");
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->server.type == SERVER_TYPE_FILE)
      {
        // insert new file server section
        String_format(line,"[file-server %'S]",serverNode->server.name);
        StringList_insert(&configLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(CONFIG_VALUES,"file-server",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 &serverNode->server
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
      if (serverNode->server.type == SERVER_TYPE_FTP)
      {
        // insert new ftp server section
        StringList_insertCString(&configLinesList,"",nextStringNode);
        StringList_insertCString(&configLinesList,"# ----------------------------------------------------------------------",nextStringNode);
        StringList_insertCString(&configLinesList,"# FTP login settings",nextStringNode);
        String_format(line,"[ftp-server %'S]",serverNode->server.name);
        StringList_insert(&configLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(CONFIG_VALUES,"ftp-server",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 &serverNode->server
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
      if (serverNode->server.type == SERVER_TYPE_SSH)
      {
        // insert new ssh-server section
        StringList_insertCString(&configLinesList,"",nextStringNode);
        StringList_insertCString(&configLinesList,"# ----------------------------------------------------------------------",nextStringNode);
        StringList_insertCString(&configLinesList,"# SSH/SCP/SFTP login settings",nextStringNode);
        String_format(line,"[ssh-server %'S]",serverNode->server.name);
        StringList_insert(&configLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(CONFIG_VALUES,"ssh-server",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 &serverNode->server
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
      if (serverNode->server.type == SERVER_TYPE_WEBDAV)
      {
        // insert new webdav-server sections
        StringList_insertCString(&configLinesList,"",nextStringNode);
        StringList_insertCString(&configLinesList,"# ----------------------------------------------------------------------",nextStringNode);
        StringList_insertCString(&configLinesList,"# WebDAV login settings",nextStringNode);
        String_format(line,"[webdav-server %'S]",serverNode->server.name);
        StringList_insert(&configLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(CONFIG_VALUES,"webdav-server",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 &serverNode->server
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
    StringList_insertCString(&configLinesList,"",nextStringNode);
    StringList_insertCString(&configLinesList,"# ----------------------------------------------------------------------",nextStringNode);
    StringList_insertCString(&configLinesList,"# master settings",nextStringNode);
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

/*---------------------------------------------------------------------*/

const char *getPasswordTypeText(PasswordTypes passwordType)
{
  const char *text;

  text = NULL;
  switch (passwordType)
  {
    case PASSWORD_TYPE_CRYPT:  text = "crypt";  break;
    case PASSWORD_TYPE_FTP:    text = "FTP";    break;
    case PASSWORD_TYPE_SSH:    text = "SSH";    break;
    case PASSWORD_TYPE_WEBDAV: text = "webDAV"; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return text;
}

void vprintInfo(uint verboseLevel, const char *prefix, const char *format, va_list arguments)
{
  String line;

  assert(format != NULL);

  if (isPrintInfo(verboseLevel))
  {
    line = String_new();

    // format line
    if (prefix != NULL) String_appendCString(line,prefix);
    String_appendVFormat(line,format,arguments);

    // output
    SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      outputConsole(stdout,line);
    }

    String_delete(line);
  }
}

void pprintInfo(uint verboseLevel, const char *prefix, const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  va_start(arguments,format);
  vprintInfo(verboseLevel,prefix,format,arguments);
  va_end(arguments);
}

void printInfo(uint verboseLevel, const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  va_start(arguments,format);
  vprintInfo(verboseLevel,NULL,format,arguments);
  va_end(arguments);
}

bool lockConsole(void)
{
  return Semaphore_lock(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
}

void unlockConsole(void)
{
  assert(Semaphore_isLocked(&consoleLock));

  Semaphore_unlock(&consoleLock);
}

void saveConsole(FILE *file, String *saveLine)
{
  ulong  i;
  size_t bytesWritten;

  assert(file != NULL);
  assert(saveLine != NULL);
  assert(Semaphore_isLocked(&consoleLock));

  (*saveLine) = String_new();

  if (File_isTerminal(file))
  {
    // wipe-out last line
    if (lastOutputLine != NULL)
    {
      for (i = 0; i < String_length(lastOutputLine); i++)
      {
        bytesWritten = fwrite("\b",1,1,file);
      }
      for (i = 0; i < String_length(lastOutputLine); i++)
      {
        bytesWritten = fwrite(" ",1,1,file);
      }
      for (i = 0; i < String_length(lastOutputLine); i++)
      {
        bytesWritten = fwrite("\b",1,1,file);
      }
      fflush(file);
    }

    // save last line
    String_set(*saveLine,lastOutputLine);
  }

  UNUSED_VARIABLE(bytesWritten);
}

void restoreConsole(FILE *file, const String *saveLine)
{
  assert(file != NULL);
  assert(saveLine != NULL);
  assert(Semaphore_isLocked(&consoleLock));

  if (File_isTerminal(file))
  {
    // force restore of line on next output
    lastOutputLine = NULL;
  }

  String_delete(*saveLine);
}

void printConsole(FILE *file, uint width, const char *format, ...)
{
  String  line;
  va_list arguments;

  assert(file != NULL);
  assert(format != NULL);

  line = String_new();

  // format line
  va_start(arguments,format);
  String_vformat(line,format,arguments);
  va_end(arguments);
  if (width > 0)
  {
    if (String_length(line) < width)
    {
      String_padRight(line,width,' ');
    }
    else
    {
      String_truncate(line,STRING_BEGIN,width);
    }
  }

  // output
  SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    outputConsole(file,line);
  }

  String_delete(line);
}

void printWarning(const char *text, ...)
{
  va_list arguments;
  String  line;
  String  saveLine;
  size_t  bytesWritten;

  assert(text != NULL);

  // output log line
  va_start(arguments,text);
  vlogMessage(NULL,LOG_TYPE_WARNING,"Warning",text,arguments);
  va_end(arguments);

  // output line
  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"Warning: ");
  String_appendVFormat(line,text,arguments);
  String_appendChar(line,'\n');
  va_end(arguments);

  SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    saveConsole(stderr,&saveLine);
    bytesWritten = fwrite(String_cString(line),1,String_length(line),stderr);
    restoreConsole(stderr,&saveLine);
    UNUSED_VARIABLE(bytesWritten);
  }
  String_delete(line);
}

void printError(const char *text, ...)
{
  va_list arguments;
  String  saveLine;
  String  line;
  size_t  bytesWritten;

  assert(text != NULL);

  // output log line
  va_start(arguments,text);
  vlogMessage(NULL,LOG_TYPE_ERROR,"ERROR",text,arguments);
  va_end(arguments);

  // output line
  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"ERROR: ");
  String_appendVFormat(line,text,arguments);
  String_appendChar(line,'\n');
  va_end(arguments);

  SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    saveConsole(stderr,&saveLine);
    bytesWritten = fwrite(String_cString(line),1,String_length(line),stderr);
    restoreConsole(stderr,&saveLine);
    UNUSED_VARIABLE(bytesWritten);
  }
  String_delete(line);
}

void executeIOOutput(ConstString line,
                     void        *userData
                    )
{
  StringList *stringList = (StringList*)userData;

  assert(line != NULL);

  printInfo(4,"%s\n",String_cString(line));
  if (stringList != NULL) StringList_append(stringList,line);
}

Errors initLog(LogHandle *logHandle)
{
  Errors error;

  assert(logHandle != NULL);

  logHandle->logFileName = String_new();
  error = File_getTmpFileNameCString(logHandle->logFileName,"bar-log",NULL /* directory */);
  if (error != ERROR_NONE)
  {
    String_delete(logHandle->logFileName); logHandle->logFileName = NULL;
    return error;
  }
  logHandle->logFile = fopen(String_cString(logHandle->logFileName),"w");
  if (logHandle->logFile == NULL)
  {
    error = ERRORX_(CREATE_FILE,errno,"%s",String_cString(logHandle->logFileName));
    (void)File_delete(logHandle->logFileName,FALSE);
    String_delete(logHandle->logFileName); logHandle->logFileName = NULL;
    return error;
  }

  return ERROR_NONE;
}

void doneLog(LogHandle *logHandle)
{
  assert(logHandle != NULL);

  if (logHandle->logFile != NULL)
  {
    fclose(logHandle->logFile); logHandle->logFile = NULL;
    (void)File_delete(logHandle->logFileName,FALSE);
    String_delete(logHandle->logFileName); logHandle->logFileName = NULL;
  }
}

void vlogMessage(LogHandle *logHandle, ulong logType, const char *prefix, const char *text, va_list arguments)
{
  static uint64 lastReopenTimestamp = 0LL;

  String  dateTime;
  va_list tmpArguments;
  uint64  nowTimestamp;

  assert(text != NULL);

  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if ((logHandle != NULL) || (logFile != NULL))
    {
      if ((logType == LOG_TYPE_ALWAYS) || ((logTypes & logType) != 0))
      {
        dateTime = Misc_formatDateTime(String_new(),Misc_getCurrentDateTime(),logFormat);

        // log to session log file
        if (logHandle != NULL)
        {
          // append to job log file (if possible)
          if (logHandle->logFile != NULL)
          {
            (void)fprintf(logHandle->logFile,"%s> ",String_cString(dateTime));
            if (prefix != NULL)
            {
              (void)fputs(prefix,logHandle->logFile);
              (void)fprintf(logHandle->logFile,": ");
            }
            va_copy(tmpArguments,arguments);
            (void)vfprintf(logHandle->logFile,text,tmpArguments);
            va_end(tmpArguments);
            fputc('\n',logHandle->logFile);
          }
        }

        // log to global log file
        if (logFile != NULL)
        {
          // re-open log for log-rotation
          nowTimestamp = Misc_getTimestamp();
          if (nowTimestamp > (lastReopenTimestamp+30LL*US_PER_SECOND))
          {
            reopenLog();
            lastReopenTimestamp = nowTimestamp;
          }

          // append to log file
          (void)fprintf(logFile,"%s> ",String_cString(dateTime));
          if (prefix != NULL)
          {
            (void)fputs(prefix,logFile);
            (void)fprintf(logFile,": ");
          }
          va_copy(tmpArguments,arguments);
          (void)vfprintf(logFile,text,tmpArguments);
          va_end(tmpArguments);
          fputc('\n',logFile);
          fflush(logFile);
        }

        String_delete(dateTime);
      }
    }
  }
}

void plogMessage(LogHandle *logHandle, ulong logType, const char *prefix, const char *text, ...)
{
  va_list arguments;

  assert(text != NULL);

  va_start(arguments,text);
  vlogMessage(logHandle,logType,prefix,text,arguments);
  va_end(arguments);
}

void logMessage(LogHandle *logHandle, ulong logType, const char *text, ...)
{
  va_list arguments;

  assert(text != NULL);

  va_start(arguments,text);
  vlogMessage(logHandle,logType,NULL,text,arguments);
  va_end(arguments);
}

void logLines(LogHandle *logHandle, ulong logType, const char *prefix, const StringList *lines)
{
  StringNode *stringNode;
  String     line;

  assert(lines != NULL);

  STRINGLIST_ITERATE(lines,stringNode,line)
  {
    logMessage(logHandle,logType,"%s%s",prefix,String_cString(line));
  }
}

void fatalLogMessage(const char *text, void *userData)
{
  String dateTime;

  assert(text != NULL);

  UNUSED_VARIABLE(userData);

  if (logFileName != NULL)
  {
    // try to open log file if not already open
    if (logFile == NULL)
    {
      logFile = fopen(logFileName,"a");
      if (logFile == NULL) printWarning("Cannot re-open log file '%s' (error: %s)!",logFileName,strerror(errno));
    }

    if (logFile != NULL)
    {
      dateTime = Misc_formatDateTime(String_new(),Misc_getCurrentDateTime(),logFormat);

      // append to log file
      (void)fprintf(logFile,"%s> ",String_cString(dateTime));
      (void)fputs("FATAL: ",logFile);
      (void)fputs(text,logFile);
      fflush(logFile);

      String_delete(dateTime);
    }
  }
}

const char* getHumanSizeString(char *buffer, uint bufferSize, uint64 n)
{
  assert(buffer != NULL);

  if      (n > 1024LL*1024LL*1024LL*1024LL)
  {
    stringFormat(buffer,bufferSize,"%.1fG",(double)n/(double)(1024LL*1024LL*1024LL*1024LL));
  }
  else if (n >        1024LL*1024LL*1024LL)
  {
    stringFormat(buffer,bufferSize,"%.1fG",(double)n/(double)(1024LL*1024LL*1024LL));
  }
  else if (n >               1024LL*1024LL)
  {
    stringFormat(buffer,bufferSize,"%.1fM",(double)n/(double)(1024LL*1024LL));
  }
  else if (n >                      1024LL)
  {
    stringFormat(buffer,bufferSize,"%.1fK",(double)n/(double)(1024LL));
  }
  else
  {
    stringFormat(buffer,bufferSize,"%"PRIu64,n);
  }

  return buffer;
}

void templateInit(TemplateHandle   *templateHandle,
                  const char       *templateString,
                  ExpandMacroModes expandMacroMode,
                  uint64           dateTime
                 )
{
  assert(templateHandle != NULL);

  // init variables
  templateHandle->templateString  = templateString;
  templateHandle->expandMacroMode = expandMacroMode;
  templateHandle->dateTime        = dateTime;
  templateHandle->textMacros      = NULL;
  templateHandle->textMacroCount  = 0;
}

void templateMacros(TemplateHandle   *templateHandle,
                    const TextMacro  textMacros[],
                    uint             textMacroCount
                   )
{
  TextMacro *newTextMacros;
  uint      newTextMacroCount;

  assert(templateHandle != NULL);

  // add macros
  newTextMacroCount = templateHandle->textMacroCount+textMacroCount;
  newTextMacros = (TextMacro*)realloc((void*)templateHandle->textMacros,newTextMacroCount*sizeof(TextMacro));
  if (newTextMacros == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  memcpy(&newTextMacros[templateHandle->textMacroCount],textMacros,textMacroCount*sizeof(TextMacro));
  templateHandle->textMacros     = newTextMacros;
  templateHandle->textMacroCount = newTextMacroCount;
}

String templateDone(TemplateHandle *templateHandle,
                    String         string
                   )
{
  TextMacro *newTextMacros;
  uint      newTextMacroCount;
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
  #endif /* HAVE_LOCALTIME_R */
  struct tm *tm;
  char      buffer[256];
  uint      weekNumberU,weekNumberW;
  ulong     i;
  char      format[4];
  size_t    length;
  uint      z;

  assert(templateHandle != NULL);

  // init variables
  if (string == NULL) string = String_new();

  // get local time
  #ifdef HAVE_LOCALTIME_R
    tm = localtime_r((const time_t*)&templateHandle->dateTime,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    tm = localtime(&templateHandle->dateTime);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);

  // get week numbers
  strftime(buffer,sizeof(buffer)-1,"%U",tm); buffer[sizeof(buffer)-1] = '\0';
  weekNumberU = (uint)atoi(buffer);
  strftime(buffer,sizeof(buffer)-1,"%W",tm); buffer[sizeof(buffer)-1] = '\0';
  weekNumberW = (uint)atoi(buffer);

  // add week macros
  newTextMacroCount = templateHandle->textMacroCount+4;
  newTextMacros = (TextMacro*)realloc((void*)templateHandle->textMacros,newTextMacroCount*sizeof(TextMacro));
  if (newTextMacros == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+0],"%U2",(weekNumberU%2)+1,"[12]"  );
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+1],"%U4",(weekNumberU%4)+1,"[1234]");
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+2],"%W2",(weekNumberW%2)+1,"[12]"  );
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+3],"%W4",(weekNumberW%4)+1,"[1234]");
  templateHandle->textMacros     = newTextMacros;
  templateHandle->textMacroCount = newTextMacroCount;

  // expand macros
  Misc_expandMacros(string,
                    templateHandle->templateString,
                    templateHandle->expandMacroMode,
                    templateHandle->textMacros,
                    templateHandle->textMacroCount,
                    FALSE
                   );

  // expand date/time macros, replace %% -> %
  i = 0L;
  while (i < String_length(string))
  {
    switch (String_index(string,i))
    {
      case '%':
        if ((i+1) < String_length(string))
        {
          switch (String_index(string,i+1))
          {
            case '%':
              // %% -> %
              String_remove(string,i,1);
              i += 1L;
              break;
            case 'a':
            case 'A':
            case 'b':
            case 'B':
            case 'c':
            case 'C':
            case 'd':
            case 'D':
            case 'e':
            case 'E':
            case 'F':
            case 'g':
            case 'G':
            case 'h':
            case 'H':
            case 'I':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'M':
            case 'n':
            case 'O':
            case 'p':
            case 'P':
            case 'r':
            case 'R':
            case 's':
            case 'S':
            case 't':
            case 'T':
            case 'u':
            case 'U':
            case 'V':
            case 'w':
            case 'W':
            case 'x':
            case 'X':
            case 'y':
            case 'Y':
            case 'z':
            case 'Z':
            case '+':
              // format date/time part
              switch (String_index(string,i+1))
              {
                case 'E':
                case 'O':
                  // %Ex, %Ox: extended date/time macros
                  format[0] = '%';
                  format[1] = String_index(string,i+1);
                  format[2] = String_index(string,i+2);
                  format[3] = '\0';

                  String_remove(string,i,3);
                  break;
                default:
                  // %x: date/time macros
                  format[0] = '%';
                  format[1] = String_index(string,i+1);
                  format[2] = '\0';

                  String_remove(string,i,2);
                  break;
              }
              length = strftime(buffer,sizeof(buffer)-1,format,tm); buffer[sizeof(buffer)-1] = '\0';

              // insert into string
              switch (templateHandle->expandMacroMode)
              {
                case EXPAND_MACRO_MODE_STRING:
                  String_insertBuffer(string,i,buffer,length);
                  i += length;
                  break;
                case EXPAND_MACRO_MODE_PATTERN:
                  for (z = 0 ; z < length; z++)
                  {
                    if (strchr("*+?{}():[].^$|",buffer[z]) != NULL)
                    {
                      String_insertChar(string,i,'\\');
                      i += 1L;
                    }
                    String_insertChar(string,i,buffer[z]);
                    i += 1L;
                  }
                  break;
                #ifndef NDEBUG
                  default:
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    break; /* not reached */
                  #endif /* NDEBUG */
              }
              break;
            default:
              // keep %x
              i += 2L;
              break;
          }
        }
        else
        {
          // keep % at end of string
          i += 1L;
        }
        break;
      default:
        i += 1L;
        break;
    }
  }

  // free resources
  free((void*)templateHandle->textMacros);

  return string;
}

String expandTemplate(const char       *templateString,
                      ExpandMacroModes expandMacroMode,
                      time_t           timestamp,
                      const TextMacro  textMacros[],
                      uint             textMacroCount
                     )
{
  TemplateHandle templateHandle;

  templateInit(&templateHandle,
               templateString,
               expandMacroMode,
               timestamp
              );
  templateMacros(&templateHandle,
                 textMacros,
                 textMacroCount
                );

  return templateDone(&templateHandle,
                      NULL  // string
                     );
}

Errors executeTemplate(const char       *templateString,
                       time_t           timestamp,
                       const TextMacro  textMacros[],
                       uint             textMacroCount
                      )
{
  String script;
  Errors error;

  if (!stringIsEmpty(templateString))
  {
    script = expandTemplate(templateString,
                            EXPAND_MACRO_MODE_STRING,
                            timestamp,
                            textMacros,
                            textMacroCount
                           );
    if (!String_isEmpty(script))
    {
      // execute script
      error = Misc_executeScript(String_cString(script),
                                 CALLBACK_(executeIOOutput,NULL),
                                 CALLBACK_(executeIOOutput,NULL)
                                );
      String_delete(script);
    }
    else
    {
      error = ERROR_EXPAND_TEMPLATE;
    }
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

/***********************************************************************\
* Name   : executeIOlogPostProcess
* Purpose: process log-post command stderr output
* Input  : line     - line
*          userData - strerr string list
* Output : -
* Return : -
* Notes  : string list will be shortend to last 5 entries
\***********************************************************************/

LOCAL void executeIOlogPostProcess(ConstString line,
                                   void        *userData
                                  )
{
  StringList *stringList = (StringList*)userData;

  assert(stringList != NULL);
  assert(line != NULL);

  StringList_append(stringList,line);
  while (StringList_count(stringList) > 5)
  {
    String_delete(StringList_removeFirst(stringList,NULL));
  }
}

void logPostProcess(LogHandle        *logHandle,
                    const JobOptions *jobOptions,
                    ArchiveTypes     archiveType,
                    ConstString      scheduleCustomText,
                    ConstString      jobName,
                    JobStates        jobState,
                    StorageFlags     storageFlags,
                    ConstString      message
                   )
{
  String     command;
  TextMacros (textMacros,7);
  StringList stderrList;
  Errors     error;
  StringNode *stringNode;
  String     string;

  UNUSED_VARIABLE(jobOptions);

  assert(logHandle != NULL);
  assert(jobName != NULL);
  assert(jobOptions != NULL);

  if (!stringIsEmpty(logPostCommand))
  {
    if (logHandle != NULL)
    {
      // init variables
      command = String_new();

      if (logHandle->logFile != NULL)
      {
        assert(logHandle->logFileName != NULL);

        // close job log file
        fclose(logHandle->logFile);
        assert(logHandle->logFileName != NULL);

        // log post command for job log file
        TEXT_MACROS_INIT(textMacros)
        {
          TEXT_MACRO_X_STRING ("%file",   logHandle->logFileName,                            TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_STRING ("%name",   jobName,                                           TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%type",   Archive_archiveTypeToString(archiveType),TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%T",      Archive_archiveTypeToShortString(archiveType), ".");
          TEXT_MACRO_X_STRING ("%text",   scheduleCustomText,                                TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%state",  Job_getStateText(jobState,storageFlags),           NULL);
          TEXT_MACRO_X_STRING ("%message",String_cString(message),NULL);
        }
//TODO: macro expanded 2x!
        Misc_expandMacros(command,
                          logPostCommand,
                          EXPAND_MACRO_MODE_STRING,
                          textMacros.data,
                          textMacros.count,
                          TRUE
                         );
        printInfo(2,"Log post process '%s'...\n",String_cString(command));
        assert(logHandle->logFileName != NULL);

        StringList_init(&stderrList);
        error = Misc_executeCommand(logPostCommand,
                                    textMacros.data,
                                    textMacros.count,
                                    CALLBACK_(NULL,NULL),
                                    CALLBACK_(executeIOlogPostProcess,&stderrList)
                                   );
        if (error != ERROR_NONE)
        {
          printError(_("Cannot post-process log file (error: %s)"),Error_getText(error));
          STRINGLIST_ITERATE(&stderrList,stringNode,string)
          {
            printError("  %s",String_cString(string));
          }
        }
        StringList_done(&stderrList);
      }

      // reset and reopen job log file
      if (logHandle->logFileName != NULL)
      {
        logHandle->logFile = fopen(String_cString(logHandle->logFileName),"w");
        if (logHandle->logFile == NULL)
        {
          printWarning("Cannot re-open log file '%s' (error: %s)",String_cString(logHandle->logFileName),strerror(errno));
        }
      }

      // free resources
      String_delete(command);
    }
  }
}

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

void initCertificate(Certificate *certificate)
{
  assert(certificate != NULL);

  certificate->data   = NULL;
  certificate->length = 0;
}

bool duplicateCertificate(Certificate *toCertificate, const Certificate *fromCertificate)
{
  void *data;

  assert(toCertificate != NULL);
  assert(fromCertificate != NULL);

  data = malloc(fromCertificate->length);
  if (data == NULL)
  {
    return FALSE;
  }
  memcpy(data,fromCertificate->data,fromCertificate->length);

  toCertificate->data   = data;
  toCertificate->length = fromCertificate->length;

  return TRUE;
}

void doneCertificate(Certificate *certificate)
{
  assert(certificate != NULL);

  if (certificate->data != NULL)
  {
    free(certificate->data);
  }
}

bool isCertificateAvailable(const Certificate *certificate)
{
  assert(certificate != NULL);

  return certificate->data != NULL;
}

void clearCertificate(Certificate *certificate)
{
  assert(certificate != NULL);

  if (certificate->data != NULL) free(certificate->data);
  certificate->data   = NULL;
  certificate->length = 0;
}

bool setCertificate(Certificate *certificate, const void *certificateData, uint certificateLength)
{
  void *data;

  assert(certificate != NULL);

  data = malloc(certificateLength);
  if (data == NULL)
  {
    return FALSE;
  }
  memcpy(data,certificateData,certificateLength);

  if (certificate->data != NULL) free(certificate->data);
  certificate->data   = data;
  certificate->length = certificateLength;

  return TRUE;
}

bool setCertificateString(Certificate *certificate, ConstString string)
{
  return setCertificate(certificate,String_cString(string),String_length(string));
}

void initKey(Key *key)
{
  assert(key != NULL);

  key->data   = NULL;
  key->length = 0;
}

bool setKey(Key *key, const void *data, uint length)
{
  void *newData;

  assert(key != NULL);

  newData = allocSecure(length+12);
  if (newData == NULL)
  {
    return FALSE;
  }
  memcpy(newData,data,length);

  if (key->data != NULL) freeSecure(key->data);
  key->data   = newData;
  key->length = length;

  return TRUE;
}

bool setKeyString(Key *key, ConstString string)
{
  return setKey(key,String_cString(string),String_length(string));
}

bool duplicateKey(Key *key, const Key *fromKey)
{
  uint length;
  void *data;

  assert(key != NULL);

  if (fromKey != NULL)
  {
    length = fromKey->length;
    data = allocSecure(length+12);
    if (data == NULL)
    {
      return FALSE;
    }
    memcpy(data,fromKey->data,length);
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

void doneKey(Key *key)
{
  assert(key != NULL);

  if (key->data != NULL) freeSecure(key->data);
  key->data   = NULL;
  key->length = 0;
}

void clearKey(Key *key)
{
  assert(key != NULL);

  doneKey(key);
}

bool copyKey(Key *key, const Key *fromKey)
{
  uint length;
  void *data;

  assert(key != NULL);

  if (fromKey != NULL)
  {
    length = fromKey->length;
    data = allocSecure(length+12);
    if (data == NULL)
    {
      return FALSE;
    }
    memcpy(data,fromKey->data,length);
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

void initHash(Hash *hash)
{
  assert(hash != NULL);

  hash->cryptHashAlgorithm = CRYPT_HASH_ALGORITHM_NONE;
  hash->data               = NULL;
  hash->length             = 0;
}

bool setHash(Hash *hash, const CryptHash *cryptHash)
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

void doneHash(Hash *hash)
{
  assert(hash != NULL);

  if (hash->data != NULL) freeSecure(hash->data);
  hash->cryptHashAlgorithm = CRYPT_HASH_ALGORITHM_NONE;
  hash->data               = NULL;
  hash->length             = 0;
}

void clearHash(Hash *hash)
{
  assert(hash != NULL);

  doneHash(hash);
}

bool equalsHash(Hash *hash, const CryptHash *cryptHash)
{
  assert(hash != NULL);
  assert(cryptHash != NULL);

  return    (hash->cryptHashAlgorithm == cryptHash->cryptHashAlgorithm)
         && Crypt_equalsHashBuffer(cryptHash,
                                   hash->data,
                                   hash->length
                                  );
}

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
      initKey(&server->ssh.publicKey);
      initKey(&server->ssh.privateKey);
      break;
    case SERVER_TYPE_WEBDAV:
      server->webDAV.loginName = String_new();
      Password_init(&server->webDAV.password);
      initKey(&server->webDAV.publicKey);
      initKey(&server->webDAV.privateKey);
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
      if (isKeyAvailable(&server->ssh.privateKey)) doneKey(&server->ssh.privateKey);
      if (isKeyAvailable(&server->ssh.publicKey)) doneKey(&server->ssh.publicKey);
      Password_done(&server->ssh.password);
      String_delete(server->ssh.loginName);
      break;
    case SERVER_TYPE_WEBDAV:
      if (isKeyAvailable(&server->webDAV.privateKey)) doneKey(&server->webDAV.privateKey);
      if (isKeyAvailable(&server->webDAV.publicKey)) doneKey(&server->webDAV.publicKey);
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

ServerNode *newServerNode(ConstString name, ServerTypes serverType)
{
  ServerNode *serverNode;

  assert(name != NULL);

  // allocate server node
  serverNode = LIST_NEW_NODE(ServerNode);
  if (serverNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  serverNode->id                                  = Misc_getId();
  initServer(&serverNode->server,name,serverType);
  serverNode->connection.lowPriorityRequestCount  = 0;
  serverNode->connection.highPriorityRequestCount = 0;
  serverNode->connection.count                    = 0;

  return serverNode;
}

void deleteServerNode(ServerNode *serverNode)
{
  assert(serverNode != NULL);

  freeServerNode(serverNode,NULL);
  LIST_DELETE_NODE(serverNode);
}

void freeServerNode(ServerNode *serverNode, void *userData)
{
  assert(serverNode != NULL);

  UNUSED_VARIABLE(userData);

  doneServer(&serverNode->server);
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
                                  (serverNode->server.type == SERVER_TYPE_FILE)
                               && String_startsWith(serverNode->server.name,storageSpecifier->archiveName)
                              );

        if (serverNode != NULL)
        {
          // get file server settings
          serverId = serverNode->id;
          initServer(server,serverNode->server.name,SERVER_TYPE_FILE);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand )
                                                               ? serverNode->server.writePreProcessCommand
                                                               : globalOptions.file.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand)
                                                               ? serverNode->server.writePostProcessCommand
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
                                  (serverNode->server.type == SERVER_TYPE_FTP)
                               && String_equals(serverNode->server.name,storageSpecifier->hostName)
                              );

        if (serverNode != NULL)
        {
          // get FTP server settings
          serverId  = serverNode->id;
          initServer(server,serverNode->server.name,SERVER_TYPE_FTP);
          server->ftp.loginName = String_duplicate(serverNode->server.ftp.loginName);
          Password_set(&server->ftp.password,&serverNode->server.ftp.password);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand )
                                                               ? serverNode->server.writePreProcessCommand
                                                               : globalOptions.ftp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand)
                                                               ? serverNode->server.writePostProcessCommand
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
                                  (serverNode->server.type == SERVER_TYPE_SSH)
                               && String_equals(serverNode->server.name,storageSpecifier->hostName)
                              );

        if (serverNode != NULL)
        {
          // get file server settings
          serverId  = serverNode->id;
          initServer(server,serverNode->server.name,SERVER_TYPE_SSH);
          server->ssh.loginName = String_duplicate(serverNode->server.ssh.loginName);
          server->ssh.port      = serverNode->server.ssh.port;
          Password_set(&server->ssh.password,&serverNode->server.ssh.password);
          duplicateKey(&server->ssh.publicKey,&serverNode->server.ssh.publicKey);
          duplicateKey(&server->ssh.privateKey,&serverNode->server.ssh.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand )
                                                               ? serverNode->server.writePreProcessCommand
                                                               : globalOptions.scp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand)
                                                              ? serverNode->server.writePostProcessCommand
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
                                  (serverNode->server.type == SERVER_TYPE_SSH)
                               && String_equals(serverNode->server.name,storageSpecifier->hostName)
                              );

        if (serverNode != NULL)
        {
          // get file server settings
          serverId  = serverNode->id;
          initServer(server,serverNode->server.name,SERVER_TYPE_SSH);
          server->ssh.loginName = String_duplicate(serverNode->server.ssh.loginName);
          server->ssh.port      = serverNode->server.ssh.port;
          Password_set(&server->ssh.password,&serverNode->server.ssh.password);
          duplicateKey(&server->ssh.publicKey,&serverNode->server.ssh.publicKey);
          duplicateKey(&server->ssh.privateKey,&serverNode->server.ssh.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand )
                                                               ? serverNode->server.writePreProcessCommand
                                                               : globalOptions.sftp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand)
                                                               ? serverNode->server.writePostProcessCommand
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
                                  (serverNode->server.type == SERVER_TYPE_WEBDAV)
                               && String_equals(serverNode->server.name,storageSpecifier->hostName)
                              );

        if (serverNode != NULL)
        {
          // get WebDAV server settings
          serverId = serverNode->id;
          initServer(server,serverNode->server.name,SERVER_TYPE_WEBDAV);
          server->webDAV.loginName = String_duplicate(serverNode->server.webDAV.loginName);
          Password_set(&server->webDAV.password,&serverNode->server.webDAV.password);
          duplicateKey(&server->webDAV.publicKey,&serverNode->server.webDAV.publicKey);
          duplicateKey(&server->webDAV.privateKey,&serverNode->server.webDAV.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand )
                                                               ? serverNode->server.writePreProcessCommand
                                                               : globalOptions.webdav.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand)
                                                               ? serverNode->server.writePostProcessCommand
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
                              (serverNode->server.type == SERVER_TYPE_FILE)
                           && String_startsWith(serverNode->server.name,directory)
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
                              (serverNode->server.type == SERVER_TYPE_FTP)
                           && String_equals(serverNode->server.name,hostName)
                          );

    // get FTP server settings
    String_set(ftpServer->loginName,
               ((jobOptions != NULL) && !String_isEmpty(jobOptions->ftpServer.loginName))
                 ? jobOptions->ftpServer.loginName
                 : ((serverNode != NULL)
                      ? serverNode->server.ftp.loginName
                      : globalOptions.defaultFTPServer->ftp.loginName
                   )
              );
    Password_set(&ftpServer->password,
                 ((jobOptions != NULL) && !Password_isEmpty(&jobOptions->ftpServer.password))
                   ? &jobOptions->ftpServer.password
                   : ((serverNode != NULL)
                      ? &serverNode->server.ftp.password
                      : &globalOptions.defaultFTPServer->ftp.password
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
                              (serverNode->server.type == SERVER_TYPE_SSH)
                           && String_equals(serverNode->server.name,hostName)
                          );

    // get SSH server settings
    sshServer->port       = ((jobOptions != NULL) && (jobOptions->sshServer.port != 0)                )
                              ? jobOptions->sshServer.port
                              : ((serverNode != NULL)
                                   ? serverNode->server.ssh.port
                                   : globalOptions.defaultSSHServer->ssh.port
                                );
    String_set(sshServer->loginName,
               ((jobOptions != NULL) && !String_isEmpty(jobOptions->sshServer.loginName) )
                 ? jobOptions->sshServer.loginName
                 : ((serverNode != NULL)
                      ? serverNode->server.ssh.loginName
                      : globalOptions.defaultSSHServer->ssh.loginName
                   )
              );
    Password_set(&sshServer->password,
                 ((jobOptions != NULL) && !Password_isEmpty(&jobOptions->sshServer.password))
                   ? &jobOptions->sshServer.password
                   : ((serverNode != NULL)
                        ? &serverNode->server.ssh.password
                        : &globalOptions.defaultSSHServer->ssh.password
                     )
                );
    duplicateKey(&sshServer->publicKey,
                 ((jobOptions != NULL) && isKeyAvailable(&jobOptions->sshServer.publicKey) )
                   ? &jobOptions->sshServer.publicKey
                   : ((serverNode != NULL)
                        ? &serverNode->server.ssh.publicKey
                        : &globalOptions.defaultSSHServer->ssh.publicKey
                     )
                );
    duplicateKey(&sshServer->privateKey,
                 ((jobOptions != NULL) && isKeyAvailable(&jobOptions->sshServer.privateKey))
                   ? &jobOptions->sshServer.privateKey
                   : ((serverNode != NULL)
                        ? &serverNode->server.ssh.privateKey
                        : &globalOptions.defaultSSHServer->ssh.privateKey
                     )
                );
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

void doneSSHServerSettings(SSHServer *sshServer)
{
  assert(sshServer != NULL);

  doneKey(&sshServer->privateKey);
  doneKey(&sshServer->publicKey);
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
                              (serverNode->server.type == SERVER_TYPE_WEBDAV)
                           && String_equals(serverNode->server.name,hostName)
                          );

    // get WebDAV server settings
//    webDAVServer->port       = ((jobOptions != NULL) && (jobOptions->webDAVServer.port != 0) ? jobOptions->webDAVServer.port : ((serverNode != NULL) ? serverNode->webDAVServer.port : globalOptions.defaultWebDAVServer->port );
    String_set(webDAVServer->loginName,
               ((jobOptions != NULL) && !String_isEmpty(jobOptions->webDAVServer.loginName))
                 ? jobOptions->webDAVServer.loginName
                 : ((serverNode != NULL)
                      ? serverNode->server.webDAV.loginName
                      : globalOptions.defaultWebDAVServer->webDAV.loginName
                   )
              );
    Password_set(&webDAVServer->password,
                 ((jobOptions != NULL) && !Password_isEmpty(&jobOptions->webDAVServer.password))
                   ? &jobOptions->webDAVServer.password
                   : ((serverNode != NULL)
                        ? &serverNode->server.webDAV.password
                        : &globalOptions.defaultWebDAVServer->webDAV.password
                     )
                );
    duplicateKey(&webDAVServer->publicKey,
                 ((jobOptions != NULL) && isKeyAvailable(&jobOptions->webDAVServer.publicKey))
                   ? &jobOptions->webDAVServer.publicKey
                   : ((serverNode != NULL)
                        ? &serverNode->server.webDAV.publicKey
                        : &globalOptions.defaultWebDAVServer->webDAV.publicKey
                     )
                );
    duplicateKey(&webDAVServer->privateKey,
                 ((jobOptions != NULL) && isKeyAvailable(&jobOptions->webDAVServer.privateKey))
                   ? &jobOptions->webDAVServer.privateKey
                   : ((serverNode != NULL)
                        ? &serverNode->server.webDAV.privateKey
                        : &globalOptions.defaultWebDAVServer->webDAV.privateKey
                     )
                );
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

void doneWebDAVServerSettings(WebDAVServer *webDAVServer)
{
  assert(webDAVServer != NULL);

  doneKey(&webDAVServer->privateKey);
  doneKey(&webDAVServer->publicKey);
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
    device->name                    = ((jobOptions != NULL) && (jobOptions->device.name                    != NULL)) ? jobOptions->device.name                    : ((deviceNode != NULL) ? deviceNode->device.name                    : globalOptions.defaultDevice->name                   );
    device->requestVolumeCommand    = ((jobOptions != NULL) && (jobOptions->device.requestVolumeCommand    != NULL)) ? jobOptions->device.requestVolumeCommand    : ((deviceNode != NULL) ? deviceNode->device.requestVolumeCommand    : globalOptions.defaultDevice->requestVolumeCommand   );
    device->unloadVolumeCommand     = ((jobOptions != NULL) && (jobOptions->device.unloadVolumeCommand     != NULL)) ? jobOptions->device.unloadVolumeCommand     : ((deviceNode != NULL) ? deviceNode->device.unloadVolumeCommand     : globalOptions.defaultDevice->unloadVolumeCommand    );
    device->loadVolumeCommand       = ((jobOptions != NULL) && (jobOptions->device.loadVolumeCommand       != NULL)) ? jobOptions->device.loadVolumeCommand       : ((deviceNode != NULL) ? deviceNode->device.loadVolumeCommand       : globalOptions.defaultDevice->loadVolumeCommand      );
    device->volumeSize              = ((jobOptions != NULL) && (jobOptions->device.volumeSize              != 0LL )) ? jobOptions->device.volumeSize              : ((deviceNode != NULL) ? deviceNode->device.volumeSize              : globalOptions.defaultDevice->volumeSize             );
    device->imagePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->device.imagePreProcessCommand  != NULL)) ? jobOptions->device.imagePreProcessCommand  : ((deviceNode != NULL) ? deviceNode->device.imagePreProcessCommand  : globalOptions.defaultDevice->imagePreProcessCommand );
    device->imagePostProcessCommand = ((jobOptions != NULL) && (jobOptions->device.imagePostProcessCommand != NULL)) ? jobOptions->device.imagePostProcessCommand : ((deviceNode != NULL) ? deviceNode->device.imagePostProcessCommand : globalOptions.defaultDevice->imagePostProcessCommand);
    device->imageCommand            = ((jobOptions != NULL) && (jobOptions->device.imageCommand            != NULL)) ? jobOptions->device.imageCommand            : ((deviceNode != NULL) ? deviceNode->device.imageCommand            : globalOptions.defaultDevice->imageCommand           );
    device->eccPreProcessCommand    = ((jobOptions != NULL) && (jobOptions->device.eccPreProcessCommand    != NULL)) ? jobOptions->device.eccPreProcessCommand    : ((deviceNode != NULL) ? deviceNode->device.eccPreProcessCommand    : globalOptions.defaultDevice->eccPreProcessCommand   );
    device->eccPostProcessCommand   = ((jobOptions != NULL) && (jobOptions->device.eccPostProcessCommand   != NULL)) ? jobOptions->device.eccPostProcessCommand   : ((deviceNode != NULL) ? deviceNode->device.eccPostProcessCommand   : globalOptions.defaultDevice->eccPostProcessCommand  );
    device->eccCommand              = ((jobOptions != NULL) && (jobOptions->device.eccCommand              != NULL)) ? jobOptions->device.eccCommand              : ((deviceNode != NULL) ? deviceNode->device.eccCommand              : globalOptions.defaultDevice->eccCommand             );
    device->blankCommand            = ((jobOptions != NULL) && (jobOptions->device.eccCommand              != NULL)) ? jobOptions->device.eccCommand              : ((deviceNode != NULL) ? deviceNode->device.blankCommand            : globalOptions.defaultDevice->blankCommand           );
    device->writePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->device.writePreProcessCommand  != NULL)) ? jobOptions->device.writePreProcessCommand  : ((deviceNode != NULL) ? deviceNode->device.writePreProcessCommand  : globalOptions.defaultDevice->writePreProcessCommand );
    device->writePostProcessCommand = ((jobOptions != NULL) && (jobOptions->device.writePostProcessCommand != NULL)) ? jobOptions->device.writePostProcessCommand : ((deviceNode != NULL) ? deviceNode->device.writePostProcessCommand : globalOptions.defaultDevice->writePostProcessCommand);
    device->writeCommand            = ((jobOptions != NULL) && (jobOptions->device.writeCommand            != NULL)) ? jobOptions->device.writeCommand            : ((deviceNode != NULL) ? deviceNode->device.writeCommand            : globalOptions.defaultDevice->writeCommand           );
  }
}

void doneDeviceSettings(Device *device)
{
  assert(device != NULL);

  UNUSED_VARIABLE(device);
}

bool allocateServer(uint serverId, ServerConnectionPriorities priority, long timeout)
{
  ServerNode *serverNode;
  uint       maxConnectionCount;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // find server
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
      if (serverNode == NULL)
      {
        Semaphore_unlock(&globalOptions.deviceList.lock);
        return FALSE;
      }

      // get max. number of allowed concurrent connections
      if (serverNode->server.maxConnectionCount != 0)
      {
        maxConnectionCount = serverNode->server.maxConnectionCount;
      }
      else
      {
        maxConnectionCount = 0;
        switch (serverNode->server.type)
        {
          case SERVER_TYPE_FILE:
            maxConnectionCount = MAX_UINT;
            break;
          case SERVER_TYPE_FTP:
            maxConnectionCount = defaultFTPServer.maxConnectionCount;
            break;
          case SERVER_TYPE_SSH:
            maxConnectionCount = defaultSSHServer.maxConnectionCount;
            break;
          case SERVER_TYPE_WEBDAV:
            maxConnectionCount = defaultWebDAVServer.maxConnectionCount;
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
            break;
        }
      }

      // allocate server
      switch (priority)
      {
        case SERVER_CONNECTION_PRIORITY_LOW:
          if (   (maxConnectionCount != 0)
              && (serverNode->connection.count >= maxConnectionCount)
             )
          {
            // request low priority connection
            serverNode->connection.lowPriorityRequestCount++;
            Semaphore_signalModified(&globalOptions.serverList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

            // wait for free connection
            while (serverNode->connection.count >= maxConnectionCount)
            {
              if (!Semaphore_waitModified(&globalOptions.serverList.lock,timeout))
              {
                Semaphore_unlock(&globalOptions.serverList.lock);
                return FALSE;
              }
            }

            // low priority request done
            assert(serverNode->connection.lowPriorityRequestCount > 0);
            serverNode->connection.lowPriorityRequestCount--;
          }
          break;
        case SERVER_CONNECTION_PRIORITY_HIGH:
          if (   (maxConnectionCount != 0)
              && (serverNode->connection.count >= maxConnectionCount)
             )
          {
            // request high priority connection
            serverNode->connection.highPriorityRequestCount++;
            Semaphore_signalModified(&globalOptions.serverList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

            // wait for free connection
            while (serverNode->connection.count >= maxConnectionCount)
            {
              if (!Semaphore_waitModified(&globalOptions.serverList.lock,timeout))
              {
                Semaphore_unlock(&globalOptions.serverList.lock);
                return FALSE;
              }
            }

            // high priority request done
            assert(serverNode->connection.highPriorityRequestCount > 0);
            serverNode->connection.highPriorityRequestCount--;
          }
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }

      // allocated connection
      serverNode->connection.count++;
    }
  }

  return TRUE;
}

void freeServer(uint serverId)
{
  ServerNode *serverNode;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // find server
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
      if (serverNode != NULL)
      {
        assert(serverNode->connection.count > 0);

        // free connection
        serverNode->connection.count--;
      }
    }
  }
}

bool isServerAllocationPending(uint serverId)
{
  bool       pendingFlag;
  ServerNode *serverNode;

  pendingFlag = FALSE;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
    {
      // find server
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
      if (serverNode != NULL)
      {
        pendingFlag = (serverNode->connection.highPriorityRequestCount > 0);
      }
    }
  }

  return pendingFlag;
}

MountNode *newMountNode(ConstString mountName, ConstString deviceName)
{
  assert(mountName != NULL);

  return newMountNodeCString(String_cString(mountName),
                             String_cString(deviceName)
                            );
}

MountNode *newMountNodeCString(const char *mountName, const char *deviceName)
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
  mountNode->id            = Misc_getId();
  mountNode->name          = String_newCString(mountName);
  mountNode->device        = String_newCString(deviceName);
  mountNode->mounted       = FALSE;
  mountNode->mountCount    = 0;

  return mountNode;
}

MountNode *duplicateMountNode(MountNode *fromMountNode,
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

void deleteMountNode(MountNode *mountNode)
{
  assert(mountNode != NULL);

  freeMountNode(mountNode,NULL);
  LIST_DELETE_NODE(mountNode);
}

void freeMountNode(MountNode *mountNode, void *userData)
{
  assert(mountNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(mountNode->device);
  String_delete(mountNode->name);
}

Errors mountAll(const MountList *mountList)
{
  const MountNode *mountNode;
  MountedNode     *mountedNode;
  Errors          error;

  assert(mountList != NULL);

  error = ERROR_NONE;

  SEMAPHORE_LOCKED_DO(&mountedList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    mountNode = LIST_HEAD(mountList);
    while (mountNode != NULL)
    {
      // find/add mounted node
      mountedNode = LIST_FIND(&mountedList,
                              mountedNode,
                                 String_equals(mountedNode->name,mountNode->name)
                              && String_equals(mountedNode->device,mountNode->device)
                             );
      if (mountedNode == NULL)
      {
        mountedNode = LIST_NEW_NODE(MountedNode);
        if (mountedNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
        mountedNode->name       = String_duplicate(mountNode->name);
        mountedNode->device     = String_duplicate(mountNode->device);
        mountedNode->mountCount = Device_isMounted(mountNode->name) ? 1 : 0;

        List_append(&mountedList,mountedNode);
      }

      // mount
      if (mountedNode->mountCount == 0)
      {
        if (!Device_isMounted(mountedNode->name))
        {
          if (!String_isEmpty(mountedNode->device))
          {
            error = Device_mount(globalOptions.mountDeviceCommand,mountedNode->name,mountedNode->device);
          }
          else
          {
            error = Device_mount(globalOptions.mountCommand,mountedNode->name,NULL);
          }
        }
      }
      if (error != ERROR_NONE)
      {
        break;
      }
      mountedNode->mountCount++;
      mountedNode->lastMountTimestamp = Misc_getTimestamp();

      // next
      mountNode = mountNode->next;
    }
    assert((error != ERROR_NONE) || (mountNode == NULL));

    if (error != ERROR_NONE)
    {
      assert(mountNode != NULL);

      printError("Cannot mount '%s' (error: %s)",
                 String_cString(mountNode->name),
                 Error_getText(error)
                );

      // revert mounts
      mountNode = mountNode->prev;
      while (mountNode != NULL)
      {
        // find mounted node
        mountedNode = LIST_FIND(&mountedList,
                                mountedNode,
                                   String_equals(mountedNode->name,mountNode->name)
                                && String_equals(mountedNode->device,mountNode->device)
                               );
        if (mountedNode != NULL)
        {
          assert(mountedNode->mountCount > 0);
          mountedNode->mountCount--;
          if (mountedNode->mountCount == 0)
          {
            (void)Device_umount(globalOptions.unmountCommand,mountNode->name);

            List_removeAndFree(&mountedList,mountedNode,CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL));
          }
        }

        // previous
        mountNode = mountNode->prev;
      }
    }
  }

  return error;
}

Errors unmountAll(const MountList *mountList)
{
  MountNode   *mountNode;
  MountedNode *mountedNode;

  assert(mountList != NULL);

  SEMAPHORE_LOCKED_DO(&mountedList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    LIST_ITERATE(mountList,mountNode)
    {
      // find mounted node
      mountedNode = LIST_FIND(&mountedList,
                              mountedNode,
                                 String_equals(mountedNode->name,mountNode->name)
                              && String_equals(mountedNode->device,mountNode->device)
                             );
      if (mountedNode != NULL)
      {
        assert(mountedNode->mountCount > 0);
        if (mountedNode->mountCount > 0) mountedNode->mountCount--;
      }
    }
  }

  return ERROR_NONE;
}

void purgeMounts(void)
{
  MountedNode *mountedNode;
  Errors      error;

  SEMAPHORE_LOCKED_DO(&mountedList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    mountedNode = mountedList.head;
    while (mountedNode != NULL)
    {
      if (   (mountedNode->mountCount == 0)
          && (Misc_getTimestamp() > (mountedNode->lastMountTimestamp+MOUNT_TIMEOUT*US_PER_MS))
         )
      {
        if (Device_isMounted(mountedNode->name))
        {
          error = Device_umount(globalOptions.unmountCommand,mountedNode->name);
          if (error != ERROR_NONE)
          {
            printWarning("Cannot unmount '%s' (error: %s)",
                         String_cString(mountedNode->name),
                         Error_getText(error)
                        );
          }
        }
        mountedNode = List_removeAndFree(&mountedList,mountedNode,CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL));
      }
      else
      {
        mountedNode = mountedNode->next;
      }
    }
  }
}

Errors getCryptPasswordFromConsole(String        name,
                                   Password      *password,
                                   PasswordTypes passwordType,
                                   const char    *text,
                                   bool          validateFlag,
                                   bool          weakCheckFlag,
                                   void          *userData
                                  )
{
  Errors error;

  assert(name == NULL);
  assert(password != NULL);

  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(userData);

  error = ERROR_UNKNOWN;

  switch (globalOptions.runMode)
  {
    case RUN_MODE_INTERACTIVE:
      {
        String title;
        String saveLine;

        SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          saveConsole(stdout,&saveLine);

          // input password
          title = String_new();
          switch (passwordType)
          {
            case PASSWORD_TYPE_CRYPT : String_format(title,"Crypt password"); break;
            case PASSWORD_TYPE_FTP   : String_format(title,"FTP password"); break;
            case PASSWORD_TYPE_SSH   : String_format(title,"SSH password"); break;
            case PASSWORD_TYPE_WEBDAV: String_format(title,"WebDAV password"); break;
          }
          if (!stringIsEmpty(text))
          {
            String_appendFormat(title,_(" for '%s'"),text);
          }
          if (!Password_input(password,String_cString(title),PASSWORD_INPUT_MODE_ANY) || (Password_length(password) <= 0))
          {
            restoreConsole(stdout,&saveLine);
            String_delete(title);
            Semaphore_unlock(&consoleLock);
            error = ERROR_NO_CRYPT_PASSWORD;
            break;
          }
          if (validateFlag)
          {
            // verify input password
            if ((text != NULL) && !stringIsEmpty(text))
            {
              String_format(title,_("Verify password for '%s'"),text);
            }
            else
            {
              String_setCString(title,"Verify password");
            }
            if (Password_inputVerify(password,String_cString(title),PASSWORD_INPUT_MODE_ANY))
            {
              error = ERROR_NONE;
            }
            else
            {
              printError(_("%s passwords are not equal!"),title);
              restoreConsole(stdout,&saveLine);
              String_delete(title);
              Semaphore_unlock(&consoleLock);
              error = ERROR_CRYPT_PASSWORDS_MISMATCH;
              break;
            }
          }
          else
          {
            error = ERROR_NONE;
          }
          String_delete(title);

          if (weakCheckFlag)
          {
            // check password quality
            if (Password_getQualityLevel(password) < MIN_PASSWORD_QUALITY_LEVEL)
            {
              printWarning(_("Low password quality!"));
            }
          }

          restoreConsole(stdout,&saveLine);
        }
      }
      break;
    case RUN_MODE_BATCH:
      printf("PASSWORD\n"); fflush(stdout);
      if (Password_input(password,NULL,PASSWORD_INPUT_MODE_CONSOLE) || (Password_length(password) <= 0))
      {
        error = ERROR_NONE;
      }
      else
      {
        error = ERROR_NO_CRYPT_PASSWORD;
      }
      break;
    case RUN_MODE_SERVER:
      error = ERROR_NO_CRYPT_PASSWORD;
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

bool parseWeekDaySet(const char *names, WeekDaySet *weekDaySet)
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

bool parseDateTimeNumber(ConstString s, int *n)
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

bool parseDateMonth(ConstString s, int *month)
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

bool configValueParsePassword(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitPassord(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (Password*)variable;
}

void configValueFormatDonePassword(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatPassword(void **formatUserData, void *userData, String line)
{
  Password *password;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  password = (Password*)(*formatUserData);
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

#if 0
//TODO: remove
bool configValueParsePasswordHash(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitPassordHash(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (*(Hash**)variable);
}

void configValueFormatDonePasswordHash(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatPasswordHash(void **formatUserData, void *userData, String line)
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

bool configValueParseCryptAlgorithms(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitCryptAlgorithms(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (CryptAlgorithms*)variable;
}

void configValueFormatDoneCryptAlgorithms(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatCryptAlgorithms(void **formatUserData, void *userData, String line)
{
  CryptAlgorithms *cryptAlgorithms;
#ifdef MULTI_CRYPT
  uint            i;
#endif /* MULTI_CRYPT */

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  cryptAlgorithms = (CryptAlgorithms*)(*formatUserData);
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

bool configValueParseBandWidth(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitBandWidth(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((BandWidthList*)variable)->head;
}

void configValueFormatDoneBandWidth(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatBandWidth(void **formatUserData, void *userData, String line)
{
  BandWidthNode *bandWidthNode;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  bandWidthNode = (BandWidthNode*)(*formatUserData);
  if (bandWidthNode != NULL)
  {
    if (bandWidthNode->fileName != NULL)
    {
      String_appendFormat(line,"%s",bandWidthNode->fileName);
    }
    else
    {
      String_appendFormat(line,"%lu",bandWidthNode->n);
    }

    (*formatUserData) = bandWidthNode->next;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool configValueParseOwner(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitOwner(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (Owner*)variable;
}

void configValueFormatDoneOwner(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatOwner(void **formatUserData, void *userData, String line)
{
  Owner *owner;
  char  userName[256],groupName[256];

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  owner = (Owner*)(*formatUserData);
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

bool configValueParsePermissions(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
    n = strlen(user);
    if      ((n >= 1) && (toupper(user[0])  == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1])  == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2])  == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
    n = strlen(group);
    if      ((n >= 1) && (toupper(group[0]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
    else if ((n >= 2) && (toupper(group[1]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
    else if ((n >= 3) && (toupper(group[2]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
    n = strlen(world);
    if      ((n >= 1) && (toupper(world[0]) == 'R')) permission |= FILE_PERMISSION_OTHER_READ;
    else if ((n >= 2) && (toupper(world[1]) == 'W')) permission |= FILE_PERMISSION_OTHER_WRITE;
    else if ((n >= 3) && (toupper(world[2]) == 'X')) permission |= FILE_PERMISSION_OTHER_EXECUTE;
  }
  else if (String_scanCString(value,"%4s:%4s",user,group))
  {
    n = strlen(user);
    if      ((n >= 1) && (toupper(user[0])  == 'R')) permission |= FILE_PERMISSION_USER_READ;
    else if ((n >= 2) && (toupper(user[1])  == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
    else if ((n >= 3) && (toupper(user[2])  == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
    n = strlen(group);
    if      ((n >= 1) && (toupper(group[0]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
    else if ((n >= 2) && (toupper(group[1]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
    else if ((n >= 3) && (toupper(group[2]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
  }
  else if (String_scanCString(value,"%4s",user))
  {
    n = strlen(user);
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

void configValueFormatInitPermissions(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (FilePermission*)variable;
}

void configValueFormatDonePermissions(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatPermissions(void **formatUserData, void *userData, String line)
{
  FilePermission *permission;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  permission = (FilePermission*)(*formatUserData);
  if (permission != NULL)
  {
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_USER_READ ) ? 'r' : '-');
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_USER_READ ) ? 'r' : '-');
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_USER_READ ) ? 'r' : '-');
    String_appendChar(line,':');
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_GROUP_READ) ? 'r' : '-');
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_GROUP_READ) ? 'r' : '-');
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_GROUP_READ) ? 'r' : '-');
    String_appendChar(line,':');
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_OTHER_READ) ? 'r' : '-');
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_OTHER_READ) ? 'r' : '-');
    String_appendChar(line,IS_SET(*permission,FILE_PERMISSION_OTHER_READ) ? 'r' : '-');

    (*formatUserData) = NULL;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

LOCAL bool configValueParseEntryPattern(EntryTypes entryType, void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

bool configValueParseFileEntryPattern(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  return configValueParseEntryPattern(ENTRY_TYPE_FILE,userData,variable,name,value,errorMessage,errorMessageSize);
}

bool configValueParseImageEntryPattern(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  return configValueParseEntryPattern(ENTRY_TYPE_IMAGE,userData,variable,name,value,errorMessage,errorMessageSize);
}

void configValueFormatInitEntryPattern(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((EntryList*)variable)->head;
}

void configValueFormatDoneEntryPattern(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatFileEntryPattern(void **formatUserData, void *userData, String line)
{
  const char* FILENAME_MAP_FROM[] = {"\n","\r","\\"};
  const char* FILENAME_MAP_TO[]   = {"\\n","\\r","\\\\"};

  EntryNode *entryNode;
  String    fileName;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  entryNode = (EntryNode*)(*formatUserData);
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

bool configValueFormatImageEntryPattern(void **formatUserData, void *userData, String line)
{
  EntryNode *entryNode;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  entryNode = (EntryNode*)(*formatUserData);
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

bool configValueParsePattern(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitPattern(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((PatternList*)variable)->head;
}

void configValueFormatDonePattern(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatPattern(void **formatUserData, void *userData, String line)
{
  PatternNode *patternNode;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  patternNode = (PatternNode*)(*formatUserData);
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

bool configValueParseMount(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitMount(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((PatternList*)variable)->head;
}

void configValueFormatDoneMount(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatMount(void **formatUserData, void *userData, String line)
{
  MountNode *mountNode;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  mountNode = (MountNode*)(*formatUserData);
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

bool configValueParseDeltaSource(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitDeltaSource(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((PatternList*)variable)->head;
}

void configValueFormatDoneDeltaSource(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatDeltaSource(void **formatUserData, void *userData, String line)
{
  DeltaSourceNode *deltaSourceNode;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  deltaSourceNode = (DeltaSourceNode*)(*formatUserData);
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

bool configValueParseCompressAlgorithms(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitCompressAlgorithms(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (CompressAlgorithmsDeltaByte*)variable;
}

void configValueFormatDoneCompressAlgorithms(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatCompressAlgorithms(void **formatUserData, void *userData, String line)
{
  CompressAlgorithmsDeltaByte *compressAlgorithmsDeltaByte;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  compressAlgorithmsDeltaByte = (CompressAlgorithmsDeltaByte*)(*formatUserData);
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

bool configValueParseCertificate(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

  if (File_existsCString(value))
  {
    // read certificate file
    error = readCertificateFile(certificate,value);
    if (error != ERROR_NONE)
    {
      return FALSE;
    }
  }
  else if (stringStartsWith(value,"base64:"))
  {
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
    certificate->data   = data;
    certificate->length = dataLength;
  }
  else
  {
    // get certificate data length
    dataLength = strlen(value);

    if (dataLength > 0)
    {
      // allocate certificate memory
      data = malloc(dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // copy data
      memcpy(data,value,dataLength);
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

bool configValueParseKeyData(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  Key        *key = (Key*)variable;
  Errors     error;
  String     string;
  FileHandle fileHandle;
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

    // read file contents
    string = String_new();
    error = File_openCString(&fileHandle,value,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      String_delete(string);
      return error;
    }
    error = File_readLine(&fileHandle,string);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(string);
      return error;
    }
    File_close(&fileHandle);

    // allocate secure memory
    dataLength = Misc_base64DecodeLength(string,STRING_BEGIN);
    data = allocSecure((size_t)dataLength);
    if (data == NULL)
    {
      (void)File_close(&fileHandle);
      return ERROR_INSUFFICIENT_MEMORY;
    }

    // decode base64
    if (!Misc_base64Decode((byte*)data,dataLength,NULL,string,STRING_BEGIN))
    {
      freeSecure(data);
      return FALSE;
    }

    // set key data
    if (key->data != NULL) freeSecure(key->data);
    key->data   = data;
    key->length = dataLength;

    // free resources
    String_delete(string);
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
        return FALSE;
      }

      // decode base64
      if (!Misc_base64DecodeCString((byte*)data,dataLength,NULL,&value[7]))
      {
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
    dataLength = strlen(value);
    if (dataLength > 0)
    {
      // allocate key memory
      data = allocSecure(dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // copy data
      memcpy(data,value,dataLength);

      // set key data
      if (key->data != NULL) freeSecure(key->data);
      key->data   = data;
      key->length = dataLength;
    }
  }

  return TRUE;
}
bool configValueParsePublicPrivateKey(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  CryptKey *cryptKey = (CryptKey*)variable;
  Errors   error;
  String   fileName;

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
    fileName = String_newCString(value);
    error = Crypt_readPublicPrivateKeyFile(cryptKey,
                                           fileName,
                                           CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                           CRYPT_KEY_DERIVE_NONE,
                                           NULL,  // cryptSalt
                                           NULL  // password
                                          );

    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,errorMessageSize,Error_getText(error));
      String_delete(fileName);
      return FALSE;
    }
    String_delete(fileName);
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

void configValueFormatInitKeyData(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (Key*)variable;
}

void configValueFormatDoneKeyData(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatKeyData(void **formatUserData, void *userData, String line)
{
  Key *key;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  if ((*formatUserData) != NULL)
  {
    key = (Key*)(*formatUserData);
    if (isKeyAvailable(key))
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

bool configValueParseHashData(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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

void configValueFormatInitHashData(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (Hash*)variable;
}

void configValueFormatDoneHashData(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatHashData(void **formatUserData, void *userData, String line)
{
  const Hash *hash;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  hash = (const Hash*)(*formatUserData);
  if (hash != NULL)
  {
    if (isHashAvailable(hash))
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

Errors initFilePattern(Pattern *pattern, ConstString fileName, PatternTypes patternType)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    String    string;
  #endif /* PLATFORM_... */
  Errors error;

  assert(pattern != NULL);
  assert(fileName != NULL);

  // init pattern
  #if   defined(PLATFORM_LINUX)
    error = Pattern_init(pattern,
                         fileName,
                         patternType,
                         PATTERN_FLAG_NONE
                        );
  #elif defined(PLATFORM_WINDOWS)
    // escape all '\' by '\\'
    string = String_duplicate(fileName);
    String_replaceAllCString(string,STRING_BEGIN,"\\","\\\\");

    error = Pattern_init(pattern,
                         string,
                         patternType,
                         PATTERN_FLAG_IGNORE_CASE
                        );

    // free resources
    String_delete(string);
  #endif /* PLATFORM_... */

  return error;
}

void initStatusInfo(StatusInfo *statusInfo)
{
  assert(statusInfo != NULL);

  statusInfo->done.count          = 0L;
  statusInfo->done.size           = 0LL;
  statusInfo->total.count         = 0L;
  statusInfo->total.size          = 0LL;
  statusInfo->collectTotalSumDone = FALSE;
  statusInfo->skipped.count       = 0L;
  statusInfo->skipped.size        = 0LL;
  statusInfo->error.count         = 0L;
  statusInfo->error.size          = 0LL;
  statusInfo->archiveSize         = 0LL;
  statusInfo->compressionRatio    = 0.0;
  statusInfo->entry.name          = String_new();
  statusInfo->entry.doneSize      = 0LL;
  statusInfo->entry.totalSize     = 0LL;
  statusInfo->storage.name        = String_new();
  statusInfo->storage.doneSize    = 0LL;
  statusInfo->storage.totalSize   = 0LL;
  statusInfo->volume.number       = 0;
  statusInfo->volume.progress     = 0.0;
  statusInfo->message             = String_new();
}

void doneStatusInfo(StatusInfo *statusInfo)
{
  assert(statusInfo != NULL);

  String_delete(statusInfo->message);
  String_delete(statusInfo->storage.name);
  String_delete(statusInfo->entry.name);
}

void setStatusInfo(StatusInfo *statusInfo, const StatusInfo *fromStatusInfo)
{
  assert(statusInfo != NULL);

  statusInfo->done.count           = fromStatusInfo->done.count;
  statusInfo->done.size            = fromStatusInfo->done.size;
  statusInfo->total.count          = fromStatusInfo->total.count;
  statusInfo->total.size           = fromStatusInfo->total.size;
  statusInfo->collectTotalSumDone  = fromStatusInfo->collectTotalSumDone;
  statusInfo->skipped.count        = fromStatusInfo->skipped.count;
  statusInfo->skipped.size         = fromStatusInfo->skipped.size;
  statusInfo->error.count          = fromStatusInfo->error.count;
  statusInfo->error.size           = fromStatusInfo->error.size;
  statusInfo->archiveSize          = fromStatusInfo->archiveSize;
  statusInfo->compressionRatio     = fromStatusInfo->compressionRatio;
  String_set(statusInfo->entry.name,fromStatusInfo->entry.name);
  statusInfo->entry.doneSize       = fromStatusInfo->entry.doneSize;
  statusInfo->entry.totalSize      = fromStatusInfo->entry.totalSize;
  String_set(statusInfo->storage.name,fromStatusInfo->storage.name);
  statusInfo->storage.doneSize     = fromStatusInfo->storage.doneSize;
  statusInfo->storage.totalSize    = fromStatusInfo->storage.totalSize;
  statusInfo->volume.number        = fromStatusInfo->volume.number;
  statusInfo->volume.progress      = fromStatusInfo->volume.progress;
  String_set(statusInfo->message,fromStatusInfo->message);
}

void resetStatusInfo(StatusInfo *statusInfo)
{
  assert(statusInfo != NULL);

  statusInfo->done.count             = 0L;
  statusInfo->done.size              = 0LL;
  statusInfo->total.count            = 0L;
  statusInfo->total.size             = 0LL;
  statusInfo->collectTotalSumDone    = FALSE;
  statusInfo->skipped.count          = 0L;
  statusInfo->skipped.size           = 0LL;
  statusInfo->error.count            = 0L;
  statusInfo->error.size             = 0LL;
  statusInfo->archiveSize            = 0LL;
  statusInfo->compressionRatio       = 0.0;
  String_clear(statusInfo->entry.name);
  statusInfo->entry.doneSize         = 0LL;
  statusInfo->entry.totalSize        = 0LL;
  String_clear(statusInfo->storage.name);
  statusInfo->storage.doneSize       = 0LL;
  statusInfo->storage.totalSize      = 0LL;
  statusInfo->volume.number          = 0;
  statusInfo->volume.progress        = 0.0;
  String_clear(statusInfo->message);
}


Errors addStorageNameListFromFile(StringList *storageNameList, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  String     line;

  assert(storageNameList != NULL);

  // init variables
  line = String_new();

  // open file
  if ((fileName == NULL) || stringEquals(fileName,"-"))
  {
    error = File_openDescriptor(&fileHandle,FILE_DESCRIPTOR_STDIN,FILE_OPEN_READ|FILE_STREAM);
  }
  else
  {
    error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  }
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // read file
  while (!File_eof(&fileHandle))
  {
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(line);
      return error;
    }
    StringList_append(storageNameList,line);
  }

  // close file
  File_close(&fileHandle);

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors addStorageNameListFromCommand(StringList *storageNameList, const char *template)
{
  String script;
  Errors error;

  assert(storageNameList != NULL);
  assert(template != NULL);

  // init variables
  script = String_new();

  // expand template
  Misc_expandMacros(script,
                    template,
                    EXPAND_MACRO_MODE_STRING,
                    NULL,  // textMacros
                    0,  // SIZE_OF_ARRAY(textMacros)
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               StringList_append(storageNameList,line);
                             },NULL),
                             CALLBACK_(NULL,NULL)
                            );
  if (error != ERROR_NONE)
  {
    String_delete(script);
    return error;
  }

  // free resources
  String_delete(script);

  return ERROR_NONE;
}

Errors addIncludeListFromFile(EntryTypes entryType, EntryList *entryList, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  String     line;

  assert(entryList != NULL);
  assert(fileName != NULL);

  // init variables
  line = String_new();

  // open file
  if (stringEquals(fileName,"-"))
  {
    error = File_openDescriptor(&fileHandle,FILE_DESCRIPTOR_STDIN,FILE_OPEN_READ|FILE_STREAM);
  }
  else
  {
    error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  }
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // read file
  while (!File_eof(&fileHandle))
  {
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(line);
      return error;
    }
    EntryList_append(entryList,entryType,line,PATTERN_TYPE_GLOB,NULL);
  }

  // close file
  File_close(&fileHandle);

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors addIncludeListFromCommand(EntryTypes entryType, EntryList *entryList, const char *template)
{
  String script;
  Errors error;

  assert(entryList != NULL);
  assert(template != NULL);

  // init variables
  script = String_new();

  // expand template
  Misc_expandMacros(script,
                    template,
                    EXPAND_MACRO_MODE_STRING,
                    NULL,  // textMacros
                    0,  // SIZE_OF_ARRAY(textMacros)
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               EntryList_append(entryList,entryType,line,PATTERN_TYPE_GLOB,NULL);
                             },NULL),
                             CALLBACK_(NULL,NULL)
                            );
  if (error != ERROR_NONE)
  {
    String_delete(script);
    return error;
  }

  // free resources
  String_delete(script);

  return ERROR_NONE;
}

Errors addExcludeListFromFile(PatternList *patternList, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  String     line;

  assert(patternList != NULL);
  assert(fileName != NULL);

  // init variables
  line = String_new();

  // open file
  // open file
  if (stringEquals(fileName,"-"))
  {
    error = File_openDescriptor(&fileHandle,FILE_DESCRIPTOR_STDIN,FILE_OPEN_READ|FILE_STREAM);
  }
  else
  {
    error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  }
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // read file
  while (!File_eof(&fileHandle))
  {
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(line);
      return error;
    }
    PatternList_append(patternList,line,PATTERN_TYPE_GLOB,NULL);
  }

  // close file
  File_close(&fileHandle);

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors addExcludeListFromCommand(PatternList *patternList, const char *template)
{
  String script;
  Errors error;

  assert(patternList != NULL);
  assert(template != NULL);

  // init variables
  script = String_new();

  // expand template
  Misc_expandMacros(script,
                    template,
                    EXPAND_MACRO_MODE_STRING,
                    NULL,  // textMacros
                    0,  // SIZE_OF_ARRAY(textMacros)
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               PatternList_append(patternList,line,PATTERN_TYPE_GLOB,NULL);
                             },NULL),
                             CALLBACK_(NULL,NULL)
                            );
  if (error != ERROR_NONE)
  {
    String_delete(script);
    return error;
  }

  // free resources
  String_delete(script);

  return ERROR_NONE;
}

bool isIncluded(const EntryNode *includeEntryNode,
                ConstString     name
               )
{
  assert(includeEntryNode != NULL);
  assert(name != NULL);

  return Pattern_match(&includeEntryNode->pattern,name,PATTERN_MATCH_MODE_BEGIN);
}

bool isInIncludedList(const EntryList *includeEntryList,
                      ConstString     name
                     )
{
  const EntryNode *entryNode;

  assert(includeEntryList != NULL);
  assert(name != NULL);

  LIST_ITERATE(includeEntryList,entryNode)
  {
    if (Pattern_match(&entryNode->pattern,name,PATTERN_MATCH_MODE_BEGIN))
    {
      return TRUE;
    }
  }

  return FALSE;
}

bool isInExcludedList(const PatternList *excludePatternList,
                      ConstString       name
                     )
{
  assert(excludePatternList != NULL);
  assert(name != NULL);

  return PatternList_match(excludePatternList,name,PATTERN_MATCH_MODE_EXACT);
}

bool hasNoBackup(ConstString pathName)
{
  String fileName;
  bool   hasNoBackupFlag;

  assert(pathName != NULL);

  hasNoBackupFlag = FALSE;

  fileName = String_new();
  hasNoBackupFlag |= File_exists(File_appendFileNameCString(File_setFileName(fileName,pathName),".nobackup"));
  hasNoBackupFlag |= File_exists(File_appendFileNameCString(File_setFileName(fileName,pathName),".NOBACKUP"));
  String_delete(fileName);

  return hasNoBackupFlag;
}

bool hasNoDumpAttribute(ConstString name)
{
  bool     hasNoDumpAttributeFlag;
  FileInfo fileInfo;

  assert(name != NULL);

  hasNoDumpAttributeFlag = FALSE;

  if (File_getInfo(&fileInfo,name))
  {
    hasNoDumpAttributeFlag = File_hasAttributeNoDump(&fileInfo);
  }

  return hasNoDumpAttributeFlag;
}

// ----------------------------------------------------------------------

#if 0
/***********************************************************************\
* Name   : readFromJob
* Purpose: read options from job file
* Input  : fileName - file name
* Output : -
* Return : TRUE iff read
* Notes  : -
\***********************************************************************/

LOCAL bool readFromJob(ConstString fileName)
{
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     title;
  String     type;
  String     name,value;
  long       nextIndex;

  assert(fileName != NULL);

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError(_("Cannot open job file '%s' (error: %s)!"),
               String_cString(fileName),
               Error_getText(error)
              );
    return FALSE;
  }

  // parse file
  failFlag = FALSE;
  line     = String_new();
  lineNb   = 0;
  title    = String_new();
  type     = String_new();
  name     = String_new();
  value    = String_new();
  while (File_getLine(&fileHandle,line,&lineNb,"#") && !failFlag)
  {
    // parse line
    if      (String_parse(line,STRING_BEGIN,"[schedule %S]",NULL,title))
    {
      // skip schedule sections
      while (   File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
             && !failFlag
            )
      {
        // nothing to do
      }
      File_ungetLine(&fileHandle,line,&lineNb);
    }
    if      (String_parse(line,STRING_BEGIN,"[persistence %S]",NULL,type))
    {
      // skip persistence sections
      while (   File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
             && !failFlag
            )
      {
        // nothing to do
      }
      File_ungetLine(&fileHandle,line,&lineNb);
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
                              JOB_CONFIG_VALUES,
                              NULL, // sectionName,
                              LAMBDA(void,(const char *errorMessage, void *userData),
                              {
                                UNUSED_VARIABLE(userData);

                                if (printInfoFlag) printf("FAIL!\n");
                                printError("%s in %s, line %ld",errorMessage,String_cString(fileName),lineNb);
                                failFlag = TRUE;
                              }),NULL,
                              LAMBDA(void,(const char *warningMessage, void *userData),
                              {
                                UNUSED_VARIABLE(userData);

                                if (printInfoFlag) printf("FAIL!\n");
                                printWarning("%s in %s, line %ld",warningMessage,String_cString(fileName),lineNb);
                              }),NULL,
                              NULL  // variable
                             );
    }
    else
    {
      printError(_("Syntax error in '%s', line %ld: '%s' - skipped"),
                 String_cString(fileName),
                 lineNb,
                 String_cString(line)
                );
    }
  }
  String_delete(value);
  String_delete(name);
  String_delete(type);
  String_delete(title);
  String_delete(line);

  // close file
  (void)File_close(&fileHandle);

  return !failFlag;
}
#endif

/***********************************************************************\
* Name   : createPIDFile
* Purpose: create pid file
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createPIDFile(void)
{
  String     fileName;
  Errors     error;
  FileHandle fileHandle;

  if (!stringIsEmpty(pidFileName))
  {
    fileName = String_new();
    error = File_open(&fileHandle,File_setFileNameCString(fileName,pidFileName),FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      printError(_("Cannot create process id file '%s' (error: %s)"),pidFileName,Error_getText(error));
      return error;
    }
    File_printLine(&fileHandle,"%d",(int)getpid());
    (void)File_close(&fileHandle);
    String_delete(fileName);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deletePIDFile
* Purpose: delete pid file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deletePIDFile(void)
{
  if (pidFileName != NULL)
  {
    (void)File_deleteCString(pidFileName,FALSE);
  }
}

/***********************************************************************\
* Name   : errorToExitcode
* Purpose: map error to exitcode
* Input  : error - error
* Output : -
* Return : exitcode
* Notes  : -
\***********************************************************************/

LOCAL int errorToExitcode(Errors error)
{
  switch (Error_getCode(error))
  {
    case ERROR_CODE_NONE:
      return EXITCODE_OK;
      break;
    case ERROR_CODE_TESTCODE:
      return EXITCODE_TESTCODE;
      break;
    case ERROR_CODE_INVALID_ARGUMENT:
      return EXITCODE_INVALID_ARGUMENT;
      break;
    case ERROR_CODE_CONFIG:
      return EXITCODE_CONFIG_ERROR;
    case ERROR_CODE_FUNCTION_NOT_SUPPORTED:
      return EXITCODE_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      return EXITCODE_FAIL;
      break;
  }
}

/***********************************************************************\
* Name   : generateEncryptionKeys
* Purpose: generate key pairs for encryption
* Input  : keyFileBaseName - key base file name or NULL
*          cryptPassword   - crypt password for private key
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors generateEncryptionKeys(const char *keyFileBaseName,
                                    Password   *cryptPassword
                                   )
{
  String   publicKeyFileName,privateKeyFileName;
  void     *data;
  uint     dataLength;
  Errors   error;
  CryptKey publicKey,privateKey;
  String   directoryName;
  size_t   bytesWritten;

  // initialize variables
  publicKeyFileName  = String_new();
  privateKeyFileName = String_new();

  if (keyFileBaseName != NULL)
  {
    // get file names of keys
    File_setFileNameCString(publicKeyFileName,keyFileBaseName);
    String_appendCString(publicKeyFileName,".public");
    File_setFileNameCString(privateKeyFileName,keyFileBaseName);
    String_appendCString(privateKeyFileName,".private");

    // check if key files already exists
    if (File_exists(publicKeyFileName))
    {
      printError(_("Public key file '%s' already exists!"),String_cString(publicKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
    if (File_exists(privateKeyFileName))
    {
      printError(_("Private key file '%s' already exists!"),String_cString(privateKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
  }

  // get crypt password for private key encryption
  if (Password_isEmpty(cryptPassword))
  {
    error = getCryptPasswordFromConsole(NULL,  // name
                                        cryptPassword,
                                        PASSWORD_TYPE_CRYPT,
                                        String_cString(privateKeyFileName),
                                        TRUE,  // validateFlag
                                        FALSE, // weakCheckFlag
                                        NULL  // userData
                                       );
    if (error != ERROR_NONE)
    {
      printError(_("No password given for private key!"));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
  }

  // generate new key pair for encryption
  printInfo(1,"Generate keys (collecting entropie)...");
  Crypt_initKey(&publicKey,CRYPT_PADDING_TYPE_NONE);
  Crypt_initKey(&privateKey,CRYPT_PADDING_TYPE_NONE);
  error = Crypt_createPublicPrivateKeyPair(&publicKey,&privateKey,generateKeyBits,generateKeyMode);
  if (error != ERROR_NONE)
  {
    printError(_("Cannot create encryption key pair (error: %s)!"),Error_getText(error));
    Crypt_doneKey(&privateKey);
    Crypt_doneKey(&publicKey);
    String_delete(privateKeyFileName);
    String_delete(publicKeyFileName);
    return error;
  }
//fprintf(stderr,"%s, %d: public %d \n",__FILE__,__LINE__,publicKey.dataLength); debugDumpMemory(publicKey.data,publicKey.dataLength,0);
//fprintf(stderr,"%s, %d: private %d\n",__FILE__,__LINE__,privateKey.dataLength); debugDumpMemory(privateKey.data,privateKey.dataLength,0);
  printInfo(1,"OK\n");

  // output keys
  if (keyFileBaseName != NULL)
  {
    // create directory if it does not exists
    directoryName = File_getDirectoryNameCString(String_new(),keyFileBaseName);
    if (!String_isEmpty(directoryName))
    {
      if      (!File_exists(directoryName))
      {
        error = File_makeDirectory(directoryName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
        if (error != ERROR_NONE)
        {
          printError(_("Cannot create directory '%s' (error: %s)!"),String_cString(directoryName),Error_getText(error));
          String_delete(directoryName);
          Crypt_doneKey(&privateKey);
          Crypt_doneKey(&publicKey);
          String_delete(privateKeyFileName);
          String_delete(publicKeyFileName);
          return error;
        }
      }
      else if (!File_isDirectory(directoryName))
      {
        printError(_("'%s' is not a directory!"),String_cString(directoryName));
        String_delete(directoryName);
        Crypt_doneKey(&privateKey);
        Crypt_doneKey(&publicKey);
        String_delete(privateKeyFileName);
        String_delete(publicKeyFileName);
        return error;
      }
    }
    String_delete(directoryName);

    // write encryption public key file
    error = Crypt_writePublicPrivateKeyFile(&publicKey,
                                            publicKeyFileName,
                                            CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // cryptSalt
                                            NULL  // password
                                           );
    if (error != ERROR_NONE)
    {
      printError(_("Cannot write encryption public key file (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created public encryption key '%s'\n",String_cString(publicKeyFileName));

    // write encryption private key file
    error = Crypt_writePublicPrivateKeyFile(&privateKey,
                                            privateKeyFileName,
                                            CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                            CRYPT_KEY_DERIVE_FUNCTION,
                                            NULL,  // cryptSalt
                                            cryptPassword
                                           );
    if (error != ERROR_NONE)
    {
      printError(_("Cannot write encryption private key file (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created private encryption key '%s'\n",String_cString(privateKeyFileName));
  }
  else
  {
    // output encryption public key to stdout
    error = Crypt_getPublicPrivateKeyData(&publicKey,
                                          &data,
                                          &dataLength,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password
                                         );
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get encryption public key (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("crypt-public-key = base64:");
    bytesWritten = fwrite(data,1,dataLength,stdout);
    printf("\n");
    freeSecure(data);

    // output encryption private key to stdout
    error = Crypt_getPublicPrivateKeyData(&privateKey,
                                          &data,
                                          &dataLength,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password
                                         );
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get encryption private key (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("crypt-private-key = base64:");
    bytesWritten = fwrite(data,1,dataLength,stdout);
    printf("\n");
    freeSecure(data);
  }
  Crypt_doneKey(&privateKey);
  Crypt_doneKey(&publicKey);

  // free resources
  String_delete(privateKeyFileName);
  String_delete(publicKeyFileName);

  UNUSED_VARIABLE(bytesWritten);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : generateSignatureKeys
* Purpose: generate key pairs for signature
* Input  : keyFileBaseName - key base file name or NULL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors generateSignatureKeys(const char *keyFileBaseName)
{
  String   publicKeyFileName,privateKeyFileName;
  void     *data;
  uint     dataLength;
  Errors   error;
  CryptKey publicKey,privateKey;
  String   directoryName;
  size_t   bytesWritten;

  // initialize variables
  publicKeyFileName  = String_new();
  privateKeyFileName = String_new();

  if (keyFileBaseName != NULL)
  {
    // get file names of keys
    File_setFileNameCString(publicKeyFileName,keyFileBaseName);
    String_appendCString(publicKeyFileName,".public");
    File_setFileNameCString(privateKeyFileName,keyFileBaseName);
    String_appendCString(privateKeyFileName,".private");

    // check if key files already exists
    if (File_exists(publicKeyFileName))
    {
      printError(_("Public key file '%s' already exists!"),String_cString(publicKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
    if (File_exists(privateKeyFileName))
    {
      printError(_("Private key file '%s' already exists!"),String_cString(privateKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
  }

  // generate new key pair for signature
  printInfo(1,"Generate keys (collecting entropie)...");
  Crypt_initKey(&publicKey,CRYPT_PADDING_TYPE_NONE);
  Crypt_initKey(&privateKey,CRYPT_PADDING_TYPE_NONE);
  error = Crypt_createPublicPrivateKeyPair(&publicKey,&privateKey,generateKeyBits,generateKeyMode);
  if (error != ERROR_NONE)
  {
    printError(_("Cannot create signature key pair (error: %s)!"),Error_getText(error));
    Crypt_doneKey(&privateKey);
    Crypt_doneKey(&publicKey);
    String_delete(privateKeyFileName);
    String_delete(publicKeyFileName);
    return error;
  }
  printInfo(1,"OK\n");

  // output keys
  if (keyFileBaseName != NULL)
  {
    // create directory if it does not exists
    directoryName = File_getDirectoryNameCString(String_new(),keyFileBaseName);
    if (!String_isEmpty(directoryName))
    {
      if      (!File_exists(directoryName))
      {
        error = File_makeDirectory(directoryName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
        if (error != ERROR_NONE)
        {
          printError(_("Cannot create directory '%s' (error: %s)!"),String_cString(directoryName),Error_getText(error));
          String_delete(directoryName);
          Crypt_doneKey(&privateKey);
          Crypt_doneKey(&publicKey);
          String_delete(privateKeyFileName);
          String_delete(publicKeyFileName);
          return error;
        }
      }
      else if (!File_isDirectory(directoryName))
      {
        printError(_("'%s' is not a directory!"),String_cString(directoryName));
        String_delete(directoryName);
        Crypt_doneKey(&privateKey);
        Crypt_doneKey(&publicKey);
        String_delete(privateKeyFileName);
        String_delete(publicKeyFileName);
        return error;
      }
    }
    String_delete(directoryName);

    // write signature public key
    error = Crypt_writePublicPrivateKeyFile(&publicKey,
                                            publicKeyFileName,
                                            CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // cryptSalt
                                            NULL  // password
                                           );
    if (error != ERROR_NONE)
    {
      printError(_("Cannot write signature public key file!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created public signature key '%s'\n",String_cString(publicKeyFileName));

    // write signature private key
    error = Crypt_writePublicPrivateKeyFile(&privateKey,
                                            privateKeyFileName,
                                            CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // cryptSalt
                                            NULL  // password,
                                           );
    if (error != ERROR_NONE)
    {
      printError(_("Cannot write signature private key file!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created private signature key '%s'\n",String_cString(privateKeyFileName));
  }
  else
  {
    // output signature public key to stdout
    error = Crypt_getPublicPrivateKeyData(&publicKey,
                                          &data,
                                          &dataLength,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password,
                                         );
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get signature public key!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("signature-public-key = base64:");
    bytesWritten = fwrite(data,1,dataLength,stdout);
    printf("\n");
    freeSecure(data);

    // output signature private key to stdout
    error = Crypt_getPublicPrivateKeyData(&privateKey,
                                          &data,
                                          &dataLength,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password
                                         );
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get signature private key!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("signature-private-key = base64:");
    bytesWritten = fwrite(data,1,dataLength,stdout);
    printf("\n");
    freeSecure(data);
  }
  Crypt_doneKey(&privateKey);
  Crypt_doneKey(&publicKey);

  // free resources
  String_delete(privateKeyFileName);
  String_delete(publicKeyFileName);

  UNUSED_VARIABLE(bytesWritten);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runDaemon
* Purpose: run as daemon
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runDaemon(void)
{
  Errors error;

  // open log file
  openLog();

  // init UUID if needed (ignore errors)
  if (String_isEmpty(uuid))
  {
    Misc_getUUID(uuid);
    (void)updateConfig();
  }

  // create pid file
  error = createPIDFile();
  if (error != ERROR_NONE)
  {
    closeLog();
    return error;
  }

  if (Continuous_isAvailable())
  {
      // init continuous
      error = Continuous_init(continuousDatabaseFileName);
      if (error != ERROR_NONE)
      {
        printError(_("Cannot initialise continuous (error: %s)!"),
                   Error_getText(error)
                  );
        deletePIDFile();
        closeLog();
        return error;
      }
  }

  // daemon mode -> run server with network
  globalOptions.runMode = RUN_MODE_SERVER;

  // run server (not detached)
  error = Server_run(serverMode,
                     serverPort,
                     serverTLSPort,
                     &serverCA,
                     &serverCert,
                     &serverKey,
                     &serverPasswordHash,
                     serverMaxConnections,
                     indexDatabaseFileName
                    );
  if (error != ERROR_NONE)
  {
    if (Continuous_isAvailable()) Continuous_done();
    deletePIDFile();
    closeLog();
    return error;
  }

  // update config
  if (configModified)
  {
    (void)updateConfig();
    configModified = FALSE;
  }

  // done continouous
  if (Continuous_isAvailable()) Continuous_done();

  // delete pid file
  deletePIDFile();

  // close log file
  closeLog();

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runBatch
* Purpose: run batch mode
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runBatch(void)
{
  Errors error;

  // batch mode
  globalOptions.runMode = RUN_MODE_BATCH;

  // batch mode -> run server with standard i/o
  error = Server_batch(STDIN_FILENO,
                       STDOUT_FILENO,
                       indexDatabaseFileName
                      );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runJob
* Purpose: run job
* Input  : jobUUID - UUID of job to execute
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runJob(ConstString jobUUIDName)
{
  const JobNode *jobNode;
  ArchiveTypes  archiveType;
  StorageFlags  storageFlags;
  JobOptions    jobOptions;
  Errors        error;

  // get job to execute
  archiveType  = ARCHIVE_TYPE_NONE;
  storageFlags = STORAGE_FLAGS_NONE;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,NO_WAIT)
  {
    // find job by name or UUID
    jobNode = NULL;
    if (jobNode == NULL) jobNode = Job_findByName(jobUUIDName);
    if (jobNode == NULL) jobNode = Job_findByUUID(jobUUIDName);
    if      (jobNode != NULL)
    {
//      String_set(jobUUID,jobNode->job.uuid);
    }
    else if (String_matchCString(jobUUIDName,STRING_BEGIN,"[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[-0-9a-fA-F]{12}",NULL,NULL))
    {
//      String_set(jobUUID,jobUUIDName);
    }
    else
    {
      printError(_("Cannot find job '%s'!"),
                 String_cString(jobUUIDName)
                );
      Job_listUnlock();
      return ERROR_CONFIG;
    }

    // get job data
    String_set(storageName,jobNode->job.storageName);
    EntryList_copy(&includeEntryList,&jobNode->job.includeEntryList,NULL,NULL);
    PatternList_copy(&excludePatternList,&jobNode->job.excludePatternList,NULL,NULL);
    archiveType  = jobNode->archiveType;
    storageFlags = jobNode->storageFlags;
    Job_duplicateOptions(&jobOptions,&jobNode->job.options);
  }

  // start job execution
  globalOptions.runMode = RUN_MODE_INTERACTIVE;

  // create archive
  error = Command_create(NULL, // masterIO
                         NULL, // job UUID
                         NULL, // schedule UUID
                         NULL, // scheduleTitle
                         NULL, // scheduleCustomText
                         storageName,
                         &includeEntryList,
                         &excludePatternList,
                         &jobOptions,
                         archiveType,
                         Misc_getCurrentDateTime(),
                         storageFlags,
                         CALLBACK_(getCryptPasswordFromConsole,NULL),
                         CALLBACK_(NULL,NULL),  // createStatusInfoFunction
                         CALLBACK_(NULL,NULL),  // storageRequestVolumeFunction
                         CALLBACK_(NULL,NULL),  // isPauseCreate
                         CALLBACK_(NULL,NULL),  // isPauseStorage
                         CALLBACK_(NULL,NULL),  // isAborted
                         NULL  // logHandle
                        );
  if (error != ERROR_NONE)
  {
    Job_doneOptions(&jobOptions);
    return error;
  }
  Job_doneOptions(&jobOptions);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runInteractive
* Purpose: run interactive
* Input  : argc - number of arguments
*          argv - arguments
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runInteractive(int argc, const char *argv[])
{
  JobOptions jobOptions;
  Errors     error;

  // get include/excluded entries from file list
  if (!String_isEmpty(globalOptions.includeFileListFileName))
  {
    error = addIncludeListFromFile(ENTRY_TYPE_FILE,&includeEntryList,String_cString(globalOptions.includeFileListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.includeImageListFileName))
  {
    error = addIncludeListFromFile(ENTRY_TYPE_IMAGE,&includeEntryList,String_cString(globalOptions.includeImageListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.excludeListFileName))
  {
    error = addExcludeListFromFile(&excludePatternList,String_cString(globalOptions.excludeListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get excluded list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }

  // get include/excluded entries from commands
  if (!String_isEmpty(globalOptions.includeFileCommand))
  {
    error = addIncludeListFromCommand(ENTRY_TYPE_FILE,&includeEntryList,String_cString(globalOptions.includeFileCommand));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.includeImageCommand))
  {
    error = addIncludeListFromCommand(ENTRY_TYPE_IMAGE,&includeEntryList,String_cString(globalOptions.includeImageCommand));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.excludeCommand))
  {
    error = addExcludeListFromCommand(&excludePatternList,String_cString(globalOptions.excludeCommand));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get excluded list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }

  // interactive mode
  globalOptions.runMode = RUN_MODE_INTERACTIVE;

  Job_initOptions(&jobOptions);
  error = ERROR_NONE;
  switch (command)
  {
    case COMMAND_CREATE_FILES:
    case COMMAND_CREATE_IMAGES:
      {
        EntryTypes entryType;
        int        i;

        // get storage name
        if (argc > 1)
        {
          String_setCString(storageName,argv[1]);
        }
        else
        {
          printError(_("No archive file name given!"));
          error = ERROR_INVALID_ARGUMENT;
        }

        // get include patterns
        if (error == ERROR_NONE)
        {
          switch (command)
          {
            case COMMAND_CREATE_FILES:  entryType = ENTRY_TYPE_FILE;  break;
            case COMMAND_CREATE_IMAGES: entryType = ENTRY_TYPE_IMAGE; break;
            default:                    entryType = ENTRY_TYPE_FILE;  break;
          }
          for (i = 2; i < argc; i++)
          {
            error = EntryList_appendCString(&includeEntryList,entryType,argv[i],globalOptions.patternType,NULL);
            if (error != ERROR_NONE)
            {
              break;
            }
          }
        }

        if (error == ERROR_NONE)
        {
          // create archive
          error = Command_create(NULL, // masterIO
                                 NULL, // job UUID
                                 NULL, // schedule UUID
                                 NULL, // scheduleTitle
                                 NULL, // scheduleCustomText
                                 storageName,
                                 &includeEntryList,
                                 &excludePatternList,
                                 &jobOptions,
                                 globalOptions.archiveType,
                                 Misc_getCurrentDateTime(),
                                 storageFlags,
                                 CALLBACK_(getCryptPasswordFromConsole,NULL),
                                 CALLBACK_(NULL,NULL),  // createStatusInfo
                                 CALLBACK_(NULL,NULL),  // storageRequestVolume
                                 CALLBACK_(NULL,NULL),  // isPauseCreate
                                 CALLBACK_(NULL,NULL),  // isPauseStorage
                                 CALLBACK_(NULL,NULL),  // isAborted
                                 NULL  // logHandle
                                );

        }

        // free resources
      }
      break;
    case COMMAND_NONE:
    case COMMAND_LIST:
    case COMMAND_TEST:
    case COMMAND_COMPARE:
    case COMMAND_RESTORE:
    case COMMAND_CONVERT:
      {
        StringList storageNameList;
        int        i;

        // get storage names
        StringList_init(&storageNameList);
        if (globalOptions.storageNameListStdin)
        {
          error = addStorageNameListFromFile(&storageNameList,NULL);
          if (error != ERROR_NONE)
          {
            printError(_("Cannot get storage names (error: %s)!"),
                       Error_getText(error)
                      );
            StringList_done(&storageNameList);
            break;
          }
        }
        if (!String_isEmpty(globalOptions.storageNameListFileName))
        {
          error = addStorageNameListFromFile(&storageNameList,String_cString(globalOptions.storageNameListFileName));
          if (error != ERROR_NONE)
          {
            printError(_("Cannot get storage names (error: %s)!"),
                       Error_getText(error)
                      );
            StringList_done(&storageNameList);
            break;
          }
        }
        if (!String_isEmpty(globalOptions.storageNameCommand))
        {
          error = addStorageNameListFromCommand(&storageNameList,String_cString(globalOptions.storageNameCommand));
          if (error != ERROR_NONE)
          {
            printError(_("Cannot get storage names (error: %s)!"),
                       Error_getText(error)
                      );
            StringList_done(&storageNameList);
            break;
          }
        }
        for (i = 1; i < argc; i++)
        {
          StringList_appendCString(&storageNameList,argv[i]);
        }

        switch (command)
        {
          case COMMAND_NONE:
            // default: info/list content
            error = Command_list(&storageNameList,
                                 &includeEntryList,
                                 &excludePatternList,
                                 !globalOptions.metaInfoFlag,  // showContentFlag
                                 &jobOptions,
                                 CALLBACK_(getCryptPasswordFromConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_LIST:
            error = Command_list(&storageNameList,
                                 &includeEntryList,
                                 &excludePatternList,
                                 TRUE,  // showContentFlag
                                 &jobOptions,
                                 CALLBACK_(getCryptPasswordFromConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_TEST:
            error = Command_test(&storageNameList,
                                 &includeEntryList,
                                 &excludePatternList,
                                 &jobOptions,
                                 CALLBACK_(getCryptPasswordFromConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_COMPARE:
            error = Command_compare(&storageNameList,
                                    &includeEntryList,
                                    &excludePatternList,
                                    &jobOptions,
                                    CALLBACK_(getCryptPasswordFromConsole,NULL),
                                    NULL  // logHandle
                                   );
            break;
          case COMMAND_RESTORE:
            error = Command_restore(&storageNameList,
                                    &includeEntryList,
                                    &excludePatternList,
                                    &jobOptions,
                                    storageFlags,
                                    CALLBACK_(NULL,NULL),  // restoreStatusInfo callback
                                    CALLBACK_(NULL,NULL),  // restoreError callback
                                    CALLBACK_(getCryptPasswordFromConsole,NULL),
                                    CALLBACK_(NULL,NULL),  // isPause callback
                                    CALLBACK_(NULL,NULL),  // isAborted callback
                                    NULL  // logHandle
                                   );
            break;
          case COMMAND_CONVERT:
            error = Command_convert(&storageNameList,
                                    jobUUID,
                                    &jobOptions,
                                    Misc_getCurrentDateTime(),
                                    CALLBACK_(getCryptPasswordFromConsole,NULL),
                                    NULL  // logHandle
                                   );
            break;
          default:
            break;
        }

        // free resources
        StringList_done(&storageNameList);
      }
      break;
    case COMMAND_GENERATE_ENCRYPTION_KEYS:
      {
        // generate new key pair for asymmetric encryption
        const char *keyFileName;

        // get key file name
        keyFileName = (argc > 1) ? argv[1] : NULL;

        // generate encrption keys
        error = generateEncryptionKeys(keyFileName,&globalOptions.cryptPassword);
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      break;
    case COMMAND_GENERATE_SIGNATURE_KEYS:
      {
        // generate new key pair for signature
        const char *keyFileName;

        // get key file name
        keyFileName = (argc > 1) ? argv[1] : NULL;

        // generate signature keys
        error = generateSignatureKeys(keyFileName);
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      break;
    default:
      printError(_("No command given!"));
      error = ERROR_INVALID_ARGUMENT;
      break;
  }
  Job_doneOptions(&jobOptions);

  return error;
}

/***********************************************************************\
* Name   : bar
* Purpose: BAR main program
* Input  : argc - number of arguments
*          argv - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors bar(int argc, const char *argv[])
{
  String        fileName;
  Errors        error;
  StringNode    *stringNode;
  const JobNode *jobNode;
  bool          printInfoFlag;

  // parse command line: pre-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,1,
                       globalOptionSet,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // output version, help
  if (versionFlag)
  {
    #ifndef NDEBUG
      printf("BAR version %s (debug)\n",VERSION_STRING);
    #else /* NDEBUG */
      printf("BAR version %s\n",VERSION_STRING);
    #endif /* not NDEBUG */

    return ERROR_NONE;
  }
  if (helpFlag || xhelpFlag || helpInternalFlag)
  {
    if      (helpInternalFlag) printUsage(argv[0],2);
    else if (xhelpFlag       ) printUsage(argv[0],1);
    else                       printUsage(argv[0],0);

    return ERROR_NONE;
  }

  if (!globalOptions.noDefaultConfigFlag)
  {
    fileName = String_new();

    // read default configuration from <CONFIG_DIR>/bar.cfg (ignore errors)
    File_setFileNameCString(fileName,CONFIG_DIR);
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName) && File_isReadable(fileName))
    {
      StringList_append(&configFileNameList,fileName);
    }

    // read default configuration from $HOME/.bar/bar.cfg (if exists)
    File_setFileNameCString(fileName,getenv("HOME"));
    File_appendFileNameCString(fileName,".bar");
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName))
    {
      StringList_append(&configFileNameList,fileName);
    }

    String_delete(fileName);
  }

  // parse command line: post-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       2,2,
                       globalOptionSet,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // if daemon: print info
  printInfoFlag = !globalOptions.quietFlag && daemonFlag;

  // read all configuration files
  STRINGLIST_ITERATE(&configFileNameList,stringNode,fileName)
  {
    if (!readConfigFile(fileName,printInfoFlag))
    {
      return ERROR_CONFIG;
    }
  }

  // if not daemon: reset verbose/quiet flag (overwrite defaults by command line options)
  if (!daemonFlag)
  {
    globalOptions.quietFlag    = FALSE;
    globalOptions.verboseLevel = 0;
  }

  // parse command line: pre+post-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,2,
                       globalOptionSet,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  if (serverMode == SERVER_MODE_MASTER)
  {
    // read jobs (if possible)
    (void)Job_rereadAll(globalOptions.jobsDirectory);

    // get UUID of job to execute
    if (!String_isEmpty(jobUUIDName))
    {
      JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,NO_WAIT)
      {
        // find job by name or UUID
        jobNode = NULL;
        if (jobNode == NULL) jobNode = Job_findByName(jobUUIDName);
        if (jobNode == NULL) jobNode = Job_findByUUID(jobUUIDName);
        if      (jobNode != NULL)
        {
          String_set(jobUUID,jobNode->job.uuid);
        }
        else if (String_matchCString(jobUUIDName,STRING_BEGIN,"[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[-0-9a-fA-F]{12}",NULL,NULL))
        {
          String_set(jobUUID,jobUUIDName);
        }
        else
        {
          printError(_("Cannot find job '%s'!"),
                     String_cString(jobUUIDName)
                    );
          Job_listUnlock();
          return ERROR_CONFIG;
        }

        // get job options

      }
    }
  }

  // parse command line: all
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       CMD_PRIORITY_ANY,CMD_PRIORITY_ANY,
                       globalOptionSet,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // check parameters
  if (!validateOptions())
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // create temporary directory
  error = File_getTmpDirectoryName(tmpDirectory,"bar",globalOptions.tmpDirectory);
  if (error != ERROR_NONE)
  {
    printError(_("Cannot create temporary directory in '%s' (error: %s)!"),
               String_cString(globalOptions.tmpDirectory),
               Error_getText(error)
              );
    return error;
  }

  // run
  error = ERROR_NONE;
  if      (daemonFlag)
  {
    error = runDaemon();
  }
  else if (batchFlag)
  {
    error = runBatch();
  }
  else if (!String_isEmpty(jobUUID) && (command == COMMAND_NONE))
  {
    error = runJob(jobUUID);
  }
  else
  {
    error = runInteractive(argc,argv);
  }

  // delete temporary directory
  (void)File_delete(tmpDirectory,TRUE);

  return error;
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  Errors error;

  assert(argc >= 0);

//FragmentList_unitTests(); exit(1);

  // init
  error = initAll();
  if (error != ERROR_NONE)
  {
    (void)fprintf(stderr,"ERROR: Cannot initialize program resources (error: %s)\n",Error_getText(error));
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return errorToExitcode(error);
  }

  // get executable name
  File_getAbsoluteFileNameCString(globalOptions.barExecutable,argv[0]);

  error = ERROR_NONE;

  // parse command line: pre-options
  if (error == ERROR_NONE)
  {
    if (!CmdOption_parse(argv,&argc,
                         COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                         0,0,
                         globalOptionSet,
                         stderr,"ERROR: ","Warning: "
                        )
       )
    {
      error = ERROR_INVALID_ARGUMENT;
    }
  }

  // read all server keys/certificates
  if (error == ERROR_NONE)
  {
    error = readAllServerKeys();
    if (error != ERROR_NONE)
    {
      printError(_("Cannot read server keys/certificates (error: %s)!"),
                 Error_getText(error)
                );
    }
  }

  // change working directory
  if (error == ERROR_NONE)
  {
    if (!stringIsEmpty(changeToDirectory))
    {
      error = File_changeDirectoryCString(changeToDirectory);
      if (error != ERROR_NONE)
      {
        printError(_("Cannot change to directory '%s' (error: %s)!"),
                   changeToDirectory,
                   Error_getText(error)
                  );
      }
    }
  }

  // run bar
  if (error == ERROR_NONE)
  {
    if (   daemonFlag
        && !noDetachFlag
        && !versionFlag
        && !helpFlag
        && !xhelpFlag
        && !helpInternalFlag
       )
    {
      // run as daemon
      #if   defined(PLATFORM_LINUX)
        // Note: do not suppress stdin/out/err for GCOV version
        #ifdef GCOV
          #define DAEMON_NO_SUPPRESS_STDIO 1
        #else /* not GCOV */
          #define DAEMON_NO_SUPPRESS_STDIO 0
        #endif /* GCOV */
        if (daemon(1,DAEMON_NO_SUPPRESS_STDIO) == 0)
        {
          error = bar(argc,argv);
        }
        else
        {
          error = ERROR_DAEMON_FAIL;
        }
      #elif defined(PLATFORM_WINDOWS)
// NYI ???
error = ERROR_STILL_NOT_IMPLEMENTED;
      #endif /* PLATFORM_... */
    }
    else
    {
      // run normal
      error = bar(argc,argv);
    }
  }

  // free resources
  doneAll();
  #ifndef NDEBUG
    debugResourceDone();
    File_debugDone();
    Array_debugDone();
    String_debugDone();
    List_debugDone();
  #endif /* not NDEBUG */

  return errorToExitcode(error);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
