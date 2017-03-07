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

#include "global.h"
#include "autofree.h"
#include "cmdoptions.h"
#include "configvalues.h"
#include "strings.h"
#include "stringlists.h"
#include "arrays.h"
#include "threads.h"

#include "errors.h"
#include "files.h"
#include "patternlists.h"
#include "entrylists.h"
#include "deltasourcelists.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "archive.h"
#include "network.h"
#include "storage.h"
#include "deltasources.h"
#include "database.h"
#include "index.h"
#include "continuous.h"
#include "misc.h"
#if HAVE_BREAKPAD
  #include "minidump.h"
#endif /* HAVE_BREAKPAD */

#include "commands_create.h"
#include "commands_list.h"
#include "commands_restore.h"
#include "commands_test.h"
#include "commands_compare.h"
#include "server.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define __VERSION_TO_STRING(z) __VERSION_TO_STRING_TMP(z)
#define __VERSION_TO_STRING_TMP(z) #z
#define VERSION_MAJOR_STRING __VERSION_TO_STRING(VERSION_MAJOR)
#define VERSION_MINOR_STRING __VERSION_TO_STRING(VERSION_MINOR)
#define VERSION_SVN_STRING __VERSION_TO_STRING(VERSION_SVN)
#define VERSION_STRING VERSION_MAJOR_STRING "." VERSION_MINOR_STRING " (rev. " VERSION_SVN_STRING ")"

#define DEFAULT_CONFIG_FILE_NAME              "bar.cfg"
#define DEFAULT_TMP_DIRECTORY                 FILE_TMP_DIRECTORY
#define DEFAULT_LOG_FORMAT                    "%Y-%m-%d %H:%M:%S"
#define DEFAULT_FRAGMENT_SIZE                 (64LL*MB)
#define DEFAULT_COMPRESS_MIN_FILE_SIZE        32
#define DEFAULT_SERVER_PORT                   38523
#ifdef HAVE_GNU_TLS
  #define DEFAULT_TLS_SERVER_PORT             38524
  #define DEFAULT_TLS_SERVER_CA_FILE          TLS_DIR "/certs/bar-ca.pem"
  #define DEFAULT_TLS_SERVER_CERTIFICATE_FILE TLS_DIR "/certs/bar-server-cert.pem"
  #define DEFAULT_TLS_SERVER_KEY_FILE         TLS_DIR "/private/bar-server-key.pem"
#else /* not HAVE_GNU_TLS */
  #define DEFAULT_TLS_SERVER_PORT             0
  #define DEFAULT_TLS_SERVER_CA_FILE          ""
  #define DEFAULT_TLS_SERVER_CERTIFICATE_FILE ""
  #define DEFAULT_TLS_SERVER_KEY_FILE         ""
#endif /* HAVE_GNU_TLS */
#define DEFAULT_MAX_SERVER_CONNECTIONS        8
#define DEFAULT_JOBS_DIRECTORY                CONFIG_DIR "/jobs"
#define DEFAULT_CD_DEVICE_NAME                "/dev/cdrw"
#define DEFAULT_DVD_DEVICE_NAME               "/dev/dvd"
#define DEFAULT_BD_DEVICE_NAME                "/dev/bd"
#define DEFAULT_DEVICE_NAME                   "/dev/raw"

#define DEFAULT_CD_VOLUME_SIZE                0LL
#define DEFAULT_DVD_VOLUME_SIZE               0LL
#define DEFAULT_BD_VOLUME_SIZE                0LL
#define DEFAULT_DEVICE_VOLUME_SIZE            0LL

#define DEFAULT_DATABASE_INDEX_FILE           "/var/lib/bar/index.db"

#define DEFAULT_VERBOSE_LEVEL                 1
#define DEFAULT_VERBOSE_LEVEL_INTERACTIVE     1

#define CD_UNLOAD_VOLUME_COMMAND              "eject %device"
#define CD_LOAD_VOLUME_COMMAND                "eject -t %device"
#define CD_IMAGE_COMMAND                      "nice mkisofs -V Backup -volset %number -r -o %image %directory"
#define CD_ECC_COMMAND                        "nice dvdisaster -mRS02 -n cd -c -i %image -v"
#define CD_BLANK_COMMAND                      "nice dvd+rw-format -blank %device"
#define CD_WRITE_COMMAND                      "nice sh -c 'mkisofs -V Backup -volset %number -r -o %image %directory && cdrecord dev=%device %image'"
#define CD_WRITE_IMAGE_COMMAND                "nice cdrecord dev=%device %image"

#define DVD_UNLOAD_VOLUME_COMMAND             "eject %device"
#define DVD_LOAD_VOLUME_COMMAND               "eject -t %device"
#define DVD_IMAGE_COMMAND                     "nice mkisofs -V Backup -volset %number -r -o %image %directory"
#define DVD_ECC_COMMAND                       "nice dvdisaster -mRS02 -n dvd -c -i %image -v"
#define DVD_BLANK_COMMAND                     "nice dvd+rw-format -blank %device"
#define DVD_WRITE_COMMAND                     "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %directory"
//#warning todo remove -dry-run
//#define DVD_WRITE_COMMAND                     "nice growisofs -Z %device -A BAR -V Backup -volset %number -dry-run -r %directory"
#define DVD_WRITE_IMAGE_COMMAND               "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload"
//#warning todo remove -dry-run
//#define DVD_WRITE_IMAGE_COMMAND               "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload -dry-run"

#define BD_UNLOAD_VOLUME_COMMAND              "eject %device"
#define BD_LOAD_VOLUME_COMMAND                "eject -t %device"
#define BD_IMAGE_COMMAND                      "nice mkisofs -V Backup -volset %number -r -o %image %directory"
#define BD_ECC_COMMAND                        "nice dvdisaster -mRS02 -n bd -c -i %image -v"
#define BD_BLANK_COMMAND                      "nice dvd+rw-format -blank %device"
#define BD_WRITE_COMMAND                      "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %directory"
//#warning todo remove -dry-run
//#define BD_WRITE_COMMAND                      "nice growisofs -Z %device -A BAR -V Backup -volset %number -dry-run -r %directory"
#define BD_WRITE_IMAGE_COMMAND                "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload"
//#define BD_WRITE_IMAGE_COMMAND                "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload  -dry-run"

#define MIN_PASSWORD_QUALITY_LEVEL            0.6

// file name extensions
#define FILE_NAME_EXTENSION_ARCHIVE_FILE      ".bar"
#define FILE_NAME_EXTENSION_INCREMENTAL_FILE  ".bid"

/***************************** Datatypes *******************************/

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

//  COMMAND_VERIFY_SIGNATURES,

  COMMAND_GENERATE_ENCRYPTION_KEYS,
  COMMAND_GENERATE_SIGNATURE_KEYS,
  COMMAND_NEW_KEY_PASSWORD,

  COMMAND_UNKNOWN,
} Commands;

/***************************** Variables *******************************/
GlobalOptions         globalOptions;
String                tmpDirectory;
Semaphore             consoleLock;
locale_t              POSIXLocale;

// Note: initialized once only here
LOCAL bool            daemonFlag       = FALSE;
LOCAL bool            noDetachFlag     = FALSE;
LOCAL bool            versionFlag      = FALSE;
LOCAL bool            helpFlag         = FALSE;
LOCAL bool            xhelpFlag        = FALSE;
LOCAL bool            helpInternalFlag = FALSE;

LOCAL Commands        command;
LOCAL String          jobName;

LOCAL JobOptions      jobOptions;
LOCAL String          uuid;
LOCAL String          storageName;
LOCAL EntryList       includeEntryList;
LOCAL const char      *includeFileCommand;
LOCAL const char      *includeImageCommand;
LOCAL PatternList     excludePatternList;
LOCAL const char      *excludeCommand;
LOCAL MountList       mountList;
LOCAL PatternList     compressExcludePatternList;
LOCAL DeltaSourceList deltaSourceList;
LOCAL Server          defaultFileServer;
LOCAL Server          defaultFTPServer;
LOCAL Server          defaultSSHServer;
LOCAL Server          defaultWebDAVServer;
LOCAL Device          defaultDevice;
LOCAL ServerModes     serverMode;
LOCAL uint            serverPort;
LOCAL uint            serverTLSPort;
LOCAL Certificate     serverCA;
LOCAL Certificate     serverCert;
LOCAL Key             serverKey;
LOCAL Password        *serverPassword;
LOCAL uint            serverMaxConnections;
LOCAL const char      *serverJobsDirectory;

LOCAL const char      *continuousDatabaseFileName;
LOCAL const char      *indexDatabaseFileName;

LOCAL ulong           logTypes;
LOCAL const char      *logFileName;
LOCAL const char      *logFormat;
LOCAL const char      *logPostCommand;

LOCAL bool            batchFlag;

LOCAL const char      *pidFileName;

LOCAL String          keyFileName;
LOCAL uint            keyBits;

/*---------------------------------------------------------------------*/

LOCAL StringList         configFileNameList;  // list of configuration files to read

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
//TODO: remove
//LOCAL bool cmdOptionParseEntryPatternCommand(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePattern(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseMount(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseDeltaSource(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseCompressAlgorithms(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseBandWidth(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseOwner(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
//TODO: multi crypt
//LOCAL bool cmdOptionParseCryptAlgorithms(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionReadCertificateFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseKeyData(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseCryptKey(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);

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
    {"lz4-0", COMPRESS_ALGORITHM_LZ4_0,NULL},
    {"lz4-1", COMPRESS_ALGORITHM_LZ4_1,NULL},
    {"lz4-2", COMPRESS_ALGORITHM_LZ4_2,NULL},
    {"lz4-3", COMPRESS_ALGORITHM_LZ4_3,NULL},
    {"lz4-4", COMPRESS_ALGORITHM_LZ4_4,NULL},
    {"lz4-5", COMPRESS_ALGORITHM_LZ4_5,NULL},
    {"lz4-6", COMPRESS_ALGORITHM_LZ4_6,NULL},
    {"lz4-7", COMPRESS_ALGORITHM_LZ4_7,NULL},
    {"lz4-8", COMPRESS_ALGORITHM_LZ4_8,NULL},
    {"lz4-9", COMPRESS_ALGORITHM_LZ4_9,NULL},
    {"lz4-10",COMPRESS_ALGORITHM_LZ4_10,NULL},
    {"lz4-11",COMPRESS_ALGORITHM_LZ4_11,NULL},
    {"lz4-12",COMPRESS_ALGORITHM_LZ4_12,NULL},
    {"lz4-13",COMPRESS_ALGORITHM_LZ4_13,NULL},
    {"lz4-14",COMPRESS_ALGORITHM_LZ4_14,NULL},
    {"lz4-15",COMPRESS_ALGORITHM_LZ4_15,NULL},
    {"lz4-16",COMPRESS_ALGORITHM_LZ4_16,NULL},
  #endif /* HAVE_LZO */
);

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS[] = CMD_VALUE_SELECT_ARRAY
(
  {"none",      CRYPT_ALGORITHM_NONE,          "no encryption"         },

  #ifdef HAVE_GCRYPT
    {"3DES",       CRYPT_ALGORITHM_3DES,       "3DES cipher"           },
    {"CAST5",      CRYPT_ALGORITHM_CAST5,      "CAST5 cipher"          },
    {"BLOWFISH",   CRYPT_ALGORITHM_BLOWFISH,   "Blowfish cipher"       },
    {"AES128",     CRYPT_ALGORITHM_AES128,     "AES cipher 128bit"     },
    {"AES192",     CRYPT_ALGORITHM_AES192,     "AES cipher 192bit"     },
    {"AES256",     CRYPT_ALGORITHM_AES256,     "AES cipher 256bit"     },
    {"TWOFISH128", CRYPT_ALGORITHM_TWOFISH128, "Twofish cipher 128bit" },
    {"TWOFISH256", CRYPT_ALGORITHM_TWOFISH256, "Twofish cipher 256bit" },
    {"SERPENT128", CRYPT_ALGORITHM_SERPENT128, "Serpent cipher 128bit" },
    {"SERPENT192", CRYPT_ALGORITHM_SERPENT192, "Serpent cipher 192bit" },
    {"SERPENT256", CRYPT_ALGORITHM_SERPENT256, "Serpent cipher 256bit" },
    {"CAMELLIA128",CRYPT_ALGORITHM_CAMELLIA128,"Camellia cipher 128bit"},
    {"CAMELLIA192",CRYPT_ALGORITHM_CAMELLIA192,"Camellia cipher 192bit"},
    {"CAMELLIA256",CRYPT_ALGORITHM_CAMELLIA256,"Camellia cipher 256bit"},
  #endif /* HAVE_GCRYPT */
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

LOCAL CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM         ("create",                       'c',0,1,command,                                         COMMAND_CREATE_FILES,                                        "create new files archives"                                                ),
  CMD_OPTION_ENUM         ("image",                        'm',0,1,command,                                         COMMAND_CREATE_IMAGES,                                       "create new images archives"                                               ),
  CMD_OPTION_ENUM         ("list",                         'l',0,1,command,                                         COMMAND_LIST,                                                "list contents of archives"                                                ),
  CMD_OPTION_ENUM         ("test",                         't',0,1,command,                                         COMMAND_TEST,                                                "test contents of archives"                                                ),
  CMD_OPTION_ENUM         ("compare",                      'd',0,1,command,                                         COMMAND_COMPARE,                                             "compare contents of archive swith files and images"                       ),
  CMD_OPTION_ENUM         ("extract",                      'x',0,1,command,                                         COMMAND_RESTORE,                                             "restore archives"                                                         ),
  CMD_OPTION_ENUM         ("convert",                      0,  0,1,command,                                         COMMAND_CONVERT,                                             "convert archives"                                                         ),
  CMD_OPTION_ENUM         ("generate-keys",                0,  0,1,command,                                         COMMAND_GENERATE_ENCRYPTION_KEYS,                            "generate new public/private key pair for encryption"                      ),
  CMD_OPTION_ENUM         ("generate-signature-keys",      0,  0,1,command,                                         COMMAND_GENERATE_SIGNATURE_KEYS,                             "generate new public/private key pair for signature"                       ),
//  CMD_OPTION_ENUM         ("new-key-password",             0,  1,1,command,                                         COMMAND_NEW_KEY_PASSWORD,                                   "set new private key password"                                             ),
  CMD_OPTION_INTEGER      ("generate-keys-bits",           0,  1,1,keyBits,                                         MIN_ASYMMETRIC_CRYPT_KEY_BITS,
                                                                                                                    MAX_ASYMMETRIC_CRYPT_KEY_BITS,COMMAND_LINE_BITS_UNITS,       "key bits",NULL                                                            ),
  CMD_OPTION_STRING       ("job",                          0,  0,1,jobName,                                                                                                      "execute job","name"                                                       ),

  CMD_OPTION_ENUM         ("normal",                       0,  1,2,jobOptions.archiveType,                          ARCHIVE_TYPE_NORMAL,                                         "create normal archive (no incremental list file)"                         ),
  CMD_OPTION_ENUM         ("full",                         'f',0,2,jobOptions.archiveType,                          ARCHIVE_TYPE_FULL,                                           "create full archive and incremental list file"                            ),
  CMD_OPTION_ENUM         ("incremental",                  'i',0,2,jobOptions.archiveType,                          ARCHIVE_TYPE_INCREMENTAL,                                    "create incremental archive"                                               ),
  CMD_OPTION_SPECIAL      ("incremental-list-file",        'I',1,2,&jobOptions.incrementalListFileName,             cmdOptionParseString,NULL,                                   "incremental list file name (default: <archive name>.bid)","file name"     ),
  CMD_OPTION_ENUM         ("differential",                 0,  1,2,jobOptions.archiveType,                          ARCHIVE_TYPE_DIFFERENTIAL,                                   "create differential archive"                                              ),

  CMD_OPTION_SELECT       ("pattern-type",                 0,  1,2,jobOptions.patternType,                          COMMAND_LINE_OPTIONS_PATTERN_TYPES,                          "select pattern type"                                                      ),

  CMD_OPTION_SPECIAL      ("include",                      '#',0,3,&includeEntryList,                               cmdOptionParseEntryPattern,NULL,                             "include pattern","pattern"                                                ),
  CMD_OPTION_CSTRING      ("include-file-command",         0,  1,3,includeFileCommand,                                                                                           "include file pattern command","command"                                   ),
  CMD_OPTION_CSTRING      ("include-image-command",        0,  1,3,includeImageCommand,                                                                                          "include image pattern command","command"                                  ),
  CMD_OPTION_SPECIAL      ("exclude",                      '!',0,3,&excludePatternList,                             cmdOptionParsePattern,NULL,                                  "exclude pattern","pattern"                                                ),
  CMD_OPTION_CSTRING      ("exclude-command",              0,  1,3,excludeCommand,                                                                                               "exclude pattern command","command"                                        ),
  CMD_OPTION_SPECIAL      ("mount",                        0  ,1,3,&mountList,                                      cmdOptionParseMount,NULL,                                    "mount device","name"                                                      ),

  CMD_OPTION_SPECIAL      ("delta-source",                 0,  0,3,&deltaSourceList,                                cmdOptionParseDeltaSource,NULL,                              "source pattern","pattern"                                                 ),

  CMD_OPTION_SPECIAL      ("config",                       0,  1,2,NULL,                                            cmdOptionParseConfigFile,NULL,                               "configuration file","file name"                                           ),

  CMD_OPTION_STRING       ("tmp-directory",                0,  1,1,globalOptions.tmpDirectory,                                                                                   "temporary directory","path"                                               ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",                 0,  1,1,globalOptions.maxTmpSize,                        0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "max. size of temporary files","unlimited"                                 ),

  CMD_OPTION_INTEGER64    ("archive-part-size",            's',0,2,jobOptions.archivePartSize,                      0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "approximated archive part size","unlimited"                               ),
  CMD_OPTION_INTEGER64    ("fragment-size",                0,  0,3,globalOptions.fragmentSize,                      0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,                    "fragment size","unlimited"                                                ),

  CMD_OPTION_INTEGER      ("directory-strip",              'p',1,2,jobOptions.directoryStripCount,                  -1,MAX_INT,NULL,                                             "number of directories to strip on extract",NULL                           ),
  CMD_OPTION_STRING       ("destination",                  0,  0,2,jobOptions.destination,                                                                                       "destination to restore entries","path"                                    ),
  CMD_OPTION_SPECIAL      ("owner",                        0,  0,2,&jobOptions.owner,                               cmdOptionParseOwner,NULL,                                    "user and group of restored files","user:group"                            ),

  CMD_OPTION_SPECIAL      ("compress-algorithm",           'z',0,2,&jobOptions.compressAlgorithms,                  cmdOptionParseCompressAlgorithms,NULL,                       "select compress algorithms to use\n"
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
                                                                                                                                                                                 #ifdef HAVE_XDELTA
                                                                                                                                                                                 "\n"
                                                                                                                                                                                 "additional select with '+':\n"
                                                                                                                                                                                 "  xdelta1..xdelta9: XDELTA compression level 1..9"
                                                                                                                                                                                 #endif
                                                                                                                                                                                 ,
                                                                                                                                                                                 "algorithm|xdelta+algorithm"                                               ),
  CMD_OPTION_INTEGER      ("compress-min-size",            0,  1,2,globalOptions.compressMinFileSize,               0,MAX_INT,COMMAND_LINE_BYTES_UNITS,                          "minimal size of file for compression",NULL                                ),
  CMD_OPTION_SPECIAL      ("compress-exclude",             0,  0,3,&compressExcludePatternList,                     cmdOptionParsePattern,NULL,                                  "exclude compression pattern","pattern"                                    ),

// TODO
#if MULTI_CRYPT
  CMD_OPTION_SPECIAL      ("crypt-algorithm",              'y',0,2,jobOptions.cryptAlgorithms,                      cmdOptionParseCryptAlgorithms,NULL,                          "select crypt algorithms to use\n"
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
#endif
  CMD_OPTION_SELECT       ("crypt-algorithm",              'y',0,2,jobOptions.cryptAlgorithms,                      COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS,                       "select crypt algorithms to use"                                           ),
  CMD_OPTION_SELECT       ("crypt-type",                   0,  0,2,jobOptions.cryptType,                            COMMAND_LINE_OPTIONS_CRYPT_TYPES,                            "select crypt type"                                                        ),
  CMD_OPTION_SPECIAL      ("crypt-password",               0,  0,2,&globalOptions.cryptPassword,                    cmdOptionParsePassword,NULL,                                 "crypt password (use with care!)","password"                               ),
  CMD_OPTION_SPECIAL      ("crypt-public-key",             0,  0,2,&jobOptions.cryptPublicKey,                      cmdOptionParseKeyData,NULL,                                  "public key for asymmetric encryption","file name|data"                    ),
  CMD_OPTION_SPECIAL      ("crypt-private-key",            0,  0,2,&jobOptions.cryptPrivateKey,                     cmdOptionParseKeyData,NULL,                                  "private key for asymmetric decryption","file name|data"                   ),
  CMD_OPTION_SPECIAL      ("signature-public-key",         0,  0,1,&globalOptions.signaturePublicKey,               cmdOptionParseCryptKey,NULL,                                 "public key for signature check","file name|data"                          ),
  CMD_OPTION_SPECIAL      ("signature-private-key",        0,  0,2,&globalOptions.signaturePrivateKey,              cmdOptionParseCryptKey,NULL,                                 "private key for signature generation","file name|data"                    ),

//TODO
//  CMD_OPTION_INTEGER64    ("file-max-storage-size",         0,  0,2,defaultFileServer.maxStorageSize,                 0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on file server","unlimited"                  ),

  CMD_OPTION_STRING       ("ftp-login-name",               0,  0,2,defaultFTPServer.ftp.loginName,                                                                               "ftp login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ftp-password",                 0,  0,2,&defaultFTPServer.ftp.password,                  cmdOptionParsePassword,NULL,                                 "ftp password (use with care!)","password"                                 ),
  CMD_OPTION_INTEGER      ("ftp-max-connections",          0,  0,2,defaultFTPServer.maxConnectionCount,             0,MAX_INT,NULL,                                              "max. number of concurrent ftp connections","unlimited"                    ),
//TODO
//  CMD_OPTION_INTEGER64    ("ftp-max-storage-size",         0,  0,2,defaultFTPServer.maxStorageSize,                 0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on ftp server","unlimited"                  ),

  CMD_OPTION_INTEGER      ("ssh-port",                     0,  0,2,defaultSSHServer.ssh.port,                       0,65535,NULL,                                                "ssh port",NULL                                                            ),
  CMD_OPTION_STRING       ("ssh-login-name",               0,  0,2,defaultSSHServer.ssh.loginName,                                                                               "ssh login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ssh-password",                 0,  0,2,&defaultSSHServer.ssh.password,                  cmdOptionParsePassword,NULL,                                 "ssh password (use with care!)","password"                                 ),
  CMD_OPTION_SPECIAL      ("ssh-public-key",               0,  1,2,&defaultSSHServer.ssh.publicKey,                 cmdOptionParseKeyData,NULL,                                  "ssh public key file name","file name|data"                                ),
  CMD_OPTION_SPECIAL      ("ssh-private-key",              0,  1,2,&defaultSSHServer.ssh.privateKey,                cmdOptionParseKeyData,NULL,                                  "ssh private key file name","file name|data"                               ),
  CMD_OPTION_INTEGER      ("ssh-max-connections",          0,  0,2,defaultSSHServer.maxConnectionCount,             0,MAX_INT,NULL,                                              "max. number of concurrent ssh connections","unlimited"                    ),
//TODO
//  CMD_OPTION_INTEGER64    ("ssh-max-storage-size",         0,  0,2,defaultSSHServer.maxStorageSize,                 0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on ssh server","unlimited"                  ),

//  CMD_OPTION_INTEGER      ("webdav-port",                  0,  0,2,defaultWebDAVServer.webDAV.port,               0,65535,NULL,                                                  "WebDAV port",NULL                                                         ),
  CMD_OPTION_STRING       ("webdav-login-name",            0,  0,2,defaultWebDAVServer.webDAV.loginName,                                                                         "WebDAV login name","name"                                                 ),
  CMD_OPTION_SPECIAL      ("webdav-password",              0,  0,2,&defaultWebDAVServer.webDAV.password,            cmdOptionParsePassword,NULL,                                 "WebDAV password (use with care!)","password"                              ),
  CMD_OPTION_INTEGER      ("webdav-max-connections",       0,  0,2,defaultWebDAVServer.maxConnectionCount,          0,MAX_INT,NULL,                                              "max. number of concurrent WebDAV connections","unlimited"                 ),
//TODO
//  CMD_OPTION_INTEGER64    ("webdav-max-storage-size",      0,  0,2,defaultWebDAVServer.maxStorageSize,              0LL,MAX_INT64,NULL,                                          "max. number of bytes to store on WebDAV server","unlimited"               ),

  CMD_OPTION_BOOLEAN      ("daemon",                       0,  1,0,daemonFlag,                                                                                                   "run in daemon mode"                                                       ),
  CMD_OPTION_BOOLEAN      ("no-detach",                    'D',1,0,noDetachFlag,                                                                                                 "do not detach in daemon mode"                                             ),
  CMD_OPTION_SELECT       ("server-mode",                  0,  1,1,serverMode,                                      COMMAND_LINE_OPTIONS_SERVER_MODES,                           "select server mode"                                                       ),
  CMD_OPTION_INTEGER      ("server-port",                  0,  1,1,serverPort,                                      0,65535,NULL,                                                "server port",NULL                                                         ),
  CMD_OPTION_INTEGER      ("server-tls-port",              0,  1,1,serverTLSPort,                                   0,65535,NULL,                                                "TLS (SSL) server port",NULL                                               ),
  CMD_OPTION_SPECIAL      ("server-ca-file",               0,  1,1,&serverCA,                                       cmdOptionReadCertificateFile,NULL,                           "TLS (SSL) server certificate authority file (CA file)","file name"        ),
  CMD_OPTION_SPECIAL      ("server-cert-file",             0,  1,1,&serverCert,                                     cmdOptionReadCertificateFile,NULL,                           "TLS (SSL) server certificate file","file name"                            ),
  CMD_OPTION_SPECIAL      ("server-key-file",              0,  1,1,&serverKey,                                      cmdOptionParseKeyData,NULL,                                      "TLS (SSL) server key file","file name|data"                               ),
  CMD_OPTION_SPECIAL      ("server-password",              0,  1,1,&serverPassword,                                 cmdOptionParsePassword,NULL,                                 "server password (use with care!)","password"                              ),
  CMD_OPTION_INTEGER      ("server-max-connections",       0,  1,1,serverMaxConnections,                            0,65535,NULL,                                                "max. concurrent connections to server",NULL                               ),
  CMD_OPTION_CSTRING      ("server-jobs-directory",        0,  1,1,serverJobsDirectory,                                                                                          "server job directory","path name"                                         ),

  CMD_OPTION_INTEGER      ("nice-level",                   0,  1,1,globalOptions.niceLevel,                         0,19,NULL,                                                   "general nice level of processes/threads",NULL                             ),
  CMD_OPTION_INTEGER      ("max-threads",                  0,  1,1,globalOptions.maxThreads,                        0,65535,NULL,                                                "max. number of concurrent compress/encryption threads","cpu cores"        ),

  CMD_OPTION_SPECIAL      ("max-band-width",               0,  1,1,&globalOptions.maxBandWidthList,                 cmdOptionParseBandWidth,NULL,                                "max. network band width to use [bits/s]","number or file name"            ),

  CMD_OPTION_BOOLEAN      ("batch",                        0,  2,1,batchFlag,                                                                                                    "run in batch mode"                                                        ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",        0,  1,1,&globalOptions.remoteBARExecutable,              cmdOptionParseString,NULL,                                   "remote BAR executable","file name"                                        ),

  CMD_OPTION_STRING       ("pre-command",                  0,  1,1,jobOptions.preProcessScript,                                                                                  "pre-process command","command"                                            ),
  CMD_OPTION_STRING       ("post-command",                 0,  1,1,jobOptions.postProcessScript,                                                                                 "post-process command","command"                                           ),

  CMD_OPTION_STRING       ("file-write-pre-command",       0,  1,1,globalOptions.file.writePreProcessCommand,                                                                    "write file pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("file-write-post-command",      0,  1,1,globalOptions.file.writePostProcessCommand,                                                                   "write file post-process command","command"                                ),

  CMD_OPTION_STRING       ("ftp-write-pre-command",        0,  1,1,globalOptions.ftp.writePreProcessCommand,                                                                     "write FTP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("ftp-write-post-command",       0,  1,1,globalOptions.ftp.writePostProcessCommand,                                                                    "write FTP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("scp-write-pre-command",        0,  1,1,globalOptions.scp.writePreProcessCommand,                                                                     "write SCP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("scp-write-post-command",       0,  1,1,globalOptions.scp.writePostProcessCommand,                                                                    "write SCP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("sftp-write-pre-command",       0,  1,1,globalOptions.sftp.writePreProcessCommand,                                                                    "write SFTP pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("sftp-write-post-command",      0,  1,1,globalOptions.sftp.writePostProcessCommand,                                                                   "write SFTP post-process command","command"                                ),

  CMD_OPTION_STRING       ("webdav-write-pre-command",     0,  1,1,globalOptions.webdav.writePreProcessCommand,                                                                  "write WebDAV pre-process command","command"                               ),
  CMD_OPTION_STRING       ("webdav-write-post-command",    0,  1,1,globalOptions.webdav.writePostProcessCommand,                                                                 "write WebDAV post-process command","command"                              ),

  CMD_OPTION_STRING       ("cd-device",                    0,  1,1,globalOptions.cd.deviceName,                                                                                  "default CD device","device name"                                          ),
  CMD_OPTION_STRING       ("cd-request-volume-command",    0,  1,1,globalOptions.cd.requestVolumeCommand,                                                                        "request new CD volume command","command"                                  ),
  CMD_OPTION_STRING       ("cd-unload-volume-command",     0,  1,1,globalOptions.cd.unloadVolumeCommand,                                                                         "unload CD volume command","command"                                       ),
  CMD_OPTION_STRING       ("cd-load-volume-command",       0,  1,1,globalOptions.cd.loadVolumeCommand,                                                                           "load CD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("cd-volume-size",               0,  1,1,globalOptions.cd.volumeSize,                     0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "CD volume size","unlimited"                                               ),
  CMD_OPTION_STRING       ("cd-image-pre-command",         0,  1,1,globalOptions.cd.imagePreProcessCommand,                                                                      "make CD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("cd-image-post-command",        0,  1,1,globalOptions.cd.imagePostProcessCommand,                                                                     "make CD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("cd-image-command",             0,  1,1,globalOptions.cd.imageCommand,                                                                                "make CD image command","command"                                          ),
  CMD_OPTION_STRING       ("cd-ecc-pre-command",           0,  1,1,globalOptions.cd.eccPreProcessCommand,                                                                        "make CD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("cd-ecc-post-command",          0,  1,1,globalOptions.cd.eccPostProcessCommand,                                                                       "make CD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("cd-ecc-command",               0,  1,1,globalOptions.cd.eccCommand,                                                                                  "make CD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("cd-blank-command",             0,  1,1,globalOptions.cd.blankCommand,                                                                                "blank CD medium command","command"                                        ),
  CMD_OPTION_STRING       ("cd-write-pre-command",         0,  1,1,globalOptions.cd.writePreProcessCommand,                                                                      "write CD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("cd-write-post-command",        0,  1,1,globalOptions.cd.writePostProcessCommand,                                                                     "write CD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("cd-write-command",             0,  1,1,globalOptions.cd.writeCommand,                                                                                "write CD command","command"                                               ),
  CMD_OPTION_STRING       ("cd-write-image-command",       0,  1,1,globalOptions.cd.writeImageCommand,                                                                           "write CD image command","command"                                         ),

  CMD_OPTION_STRING       ("dvd-device",                   0,  1,1,globalOptions.dvd.deviceName,                                                                                 "default DVD device","device name"                                         ),
  CMD_OPTION_STRING       ("dvd-request-volume-command",   0,  1,1,globalOptions.dvd.requestVolumeCommand,                                                                       "request new DVD volume command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-unload-volume-command",    0,  1,1,globalOptions.dvd.unloadVolumeCommand,                                                                        "unload DVD volume command","command"                                      ),
  CMD_OPTION_STRING       ("dvd-load-volume-command",      0,  1,1,globalOptions.dvd.loadVolumeCommand,                                                                          "load DVD volume command","command"                                        ),
  CMD_OPTION_INTEGER64    ("dvd-volume-size",              0,  1,1,globalOptions.dvd.volumeSize,                    0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "DVD volume size","unlimited"                                              ),
  CMD_OPTION_STRING       ("dvd-image-pre-command",        0,  1,1,globalOptions.dvd.imagePreProcessCommand,                                                                     "make DVD image pre-process command","command"                             ),
  CMD_OPTION_STRING       ("dvd-image-post-command",       0,  1,1,globalOptions.dvd.imagePostProcessCommand,                                                                    "make DVD image post-process command","command"                            ),
  CMD_OPTION_STRING       ("dvd-image-command",            0,  1,1,globalOptions.dvd.imageCommand,                                                                               "make DVD image command","command"                                         ),
  CMD_OPTION_STRING       ("dvd-ecc-pre-command",          0,  1,1,globalOptions.dvd.eccPreProcessCommand,                                                                       "make DVD error-correction codes pre-process command","command"            ),
  CMD_OPTION_STRING       ("dvd-ecc-post-command",         0,  1,1,globalOptions.dvd.eccPostProcessCommand,                                                                      "make DVD error-correction codes post-process command","command"           ),
  CMD_OPTION_STRING       ("dvd-ecc-command",              0,  1,1,globalOptions.dvd.eccCommand,                                                                                 "make DVD error-correction codes command","command"                        ),
  CMD_OPTION_STRING       ("dvd-blank-command",            0,  1,1,globalOptions.dvd.blankCommand,                                                                               "blank DVD mediumcommand","command"                                        ),
  CMD_OPTION_STRING       ("dvd-write-pre-command",        0,  1,1,globalOptions.dvd.writePreProcessCommand,                                                                     "write DVD pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("dvd-write-post-command",       0,  1,1,globalOptions.dvd.writePostProcessCommand,                                                                    "write DVD post-process command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-write-command",            0,  1,1,globalOptions.dvd.writeCommand,                                                                               "write DVD command","command"                                              ),
  CMD_OPTION_STRING       ("dvd-write-image-command",      0,  1,1,globalOptions.dvd.writeImageCommand,                                                                          "write DVD image command","command"                                        ),

  CMD_OPTION_STRING       ("bd-device",                    0,  1,1,globalOptions.bd.deviceName,                                                                                  "default BD device","device name"                                          ),
  CMD_OPTION_STRING       ("bd-request-volume-command",    0,  1,1,globalOptions.bd.requestVolumeCommand,                                                                        "request new BD volume command","command"                                  ),
  CMD_OPTION_STRING       ("bd-unload-volume-command",     0,  1,1,globalOptions.bd.unloadVolumeCommand,                                                                         "unload BD volume command","command"                                       ),
  CMD_OPTION_STRING       ("bd-load-volume-command",       0,  1,1,globalOptions.bd.loadVolumeCommand,                                                                           "load BD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("bd-volume-size",               0,  1,1,globalOptions.bd.volumeSize,                     0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "BD volume size","unlimited"                                               ),
  CMD_OPTION_STRING       ("bd-image-pre-command",         0,  1,1,globalOptions.bd.imagePreProcessCommand,                                                                      "make BD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("bd-image-post-command",        0,  1,1,globalOptions.bd.imagePostProcessCommand,                                                                     "make BD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("bd-image-command",             0,  1,1,globalOptions.bd.imageCommand,                                                                                "make BD image command","command"                                          ),
  CMD_OPTION_STRING       ("bd-ecc-pre-command",           0,  1,1,globalOptions.bd.eccPreProcessCommand,                                                                        "make BD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("bd-ecc-post-command",          0,  1,1,globalOptions.bd.eccPostProcessCommand,                                                                       "make BD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("bd-ecc-command",               0,  1,1,globalOptions.bd.eccCommand,                                                                                  "make BD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("bd-blank-command",             0,  1,1,globalOptions.bd.blankCommand,                                                                                "blank BD medium command","command"                                        ),
  CMD_OPTION_STRING       ("bd-write-pre-command",         0,  1,1,globalOptions.bd.writePreProcessCommand,                                                                      "write BD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("bd-write-post-command",        0,  1,1,globalOptions.bd.writePostProcessCommand,                                                                     "write BD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("bd-write-command",             0,  1,1,globalOptions.bd.writeCommand,                                                                                "write BD command","command"                                               ),
  CMD_OPTION_STRING       ("bd-write-image-command",       0,  1,1,globalOptions.bd.writeImageCommand,                                                                           "write BD image command","command"                                         ),

  CMD_OPTION_STRING       ("device",                       0,  1,1,defaultDevice.name,                                                                                           "default device","device name"                                             ),
  CMD_OPTION_STRING       ("device-request-volume-command",0,  1,1,defaultDevice.requestVolumeCommand,                                                                           "request new volume command","command"                                     ),
  CMD_OPTION_STRING       ("device-load-volume-command",   0,  1,1,defaultDevice.loadVolumeCommand,                                                                              "load volume command","command"                                            ),
  CMD_OPTION_STRING       ("device-unload-volume-command", 0,  1,1,defaultDevice.unloadVolumeCommand,                                                                            "unload volume command","command"                                          ),
  CMD_OPTION_INTEGER64    ("device-volume-size",           0,  1,1,defaultDevice.volumeSize,                        0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "volume size","unlimited"                                                  ),
  CMD_OPTION_STRING       ("device-image-pre-command",     0,  1,1,defaultDevice.imagePreProcessCommand,                                                                         "make image pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("device-image-post-command",    0,  1,1,defaultDevice.imagePostProcessCommand,                                                                        "make image post-process command","command"                                ),
  CMD_OPTION_STRING       ("device-image-command",         0,  1,1,defaultDevice.imageCommand,                                                                                   "make image command","command"                                             ),
  CMD_OPTION_STRING       ("device-ecc-pre-command",       0,  1,1,defaultDevice.eccPreProcessCommand,                                                                           "make error-correction codes pre-process command","command"                ),
  CMD_OPTION_STRING       ("device-ecc-post-command",      0,  1,1,defaultDevice.eccPostProcessCommand,                                                                          "make error-correction codes post-process command","command"               ),
  CMD_OPTION_STRING       ("device-ecc-command",           0,  1,1,defaultDevice.eccCommand,                                                                                     "make error-correction codes command","command"                            ),
  CMD_OPTION_STRING       ("device-blank-command",         0,  1,1,defaultDevice.blankCommand,                                                                                   "blank device medium command","command"                                    ),
  CMD_OPTION_STRING       ("device-write-pre-command",     0,  1,1,defaultDevice.writePreProcessCommand,                                                                         "write device pre-process command","command"                               ),
  CMD_OPTION_STRING       ("device-write-post-command",    0,  1,1,defaultDevice.writePostProcessCommand,                                                                        "write device post-process command","command"                              ),
  CMD_OPTION_STRING       ("device-write-command",         0,  1,1,defaultDevice.writeCommand,                                                                                   "write device command","command"                                           ),

  CMD_OPTION_INTEGER64    ("max-storage-size",             0,  1,2,jobOptions.maxStorageSize,                       0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "max. storage size","unlimited"                                            ),
//TODO
#if 0
  CMD_OPTION_INTEGER      ("min-keep",                     0,  1,2,jobOptions.minKeep,                              0,MAX_INT,NULL,                                              "min. keep","unlimited"                                                    ),
  CMD_OPTION_INTEGER      ("max-keep",                     0,  1,2,jobOptions.maxKeep,                              0,MAX_INT,NULL,                                              "max. keep","unlimited"                                                    ),
  CMD_OPTION_INTEGER      ("max-age",                      0,  1,2,jobOptions.maxAge,                               0,MAX_INT,NULL,                                              "max. age [days]","unlimited"                                              ),
#endif
  CMD_OPTION_INTEGER64    ("volume-size",                  0,  1,2,jobOptions.volumeSize,                           0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "volume size","unlimited"                                                  ),
  CMD_OPTION_BOOLEAN      ("ecc",                          0,  1,2,jobOptions.errorCorrectionCodesFlag,                                                                          "add error-correction codes with 'dvdisaster' tool"                        ),
  CMD_OPTION_BOOLEAN      ("always-create-image",          0,  1,2,jobOptions.alwaysCreateImageFlag,                                                                             "always create image for CD/DVD/BD/device"                                 ),
  CMD_OPTION_BOOLEAN      ("blank",                        0,  1,2,jobOptions.blankFlag,                                                                                         "blank medium before writing"                                              ),

  CMD_OPTION_STRING       ("comment",                      0,  1,1,jobOptions.comment,                                                                                           "comment","text"                                                           ),

  CMD_OPTION_CSTRING      ("continuous-database",          0,  2,1,continuousDatabaseFileName,                                                                                   "continuous database file name (default: in memory)","file name"           ),
  CMD_OPTION_INTEGER64    ("continuous-max-size",          0,  1,2,globalOptions.continuousMaxSize,                 0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                      "max. continuous size","unlimited"                                         ),

  CMD_OPTION_CSTRING      ("index-database",               0,  1,1,indexDatabaseFileName,                                                                                        "index database file name","file name"                                     ),
  CMD_OPTION_BOOLEAN      ("index-database-auto-update",   0,  1,1,globalOptions.indexDatabaseAutoUpdateFlag,                                                                    "enabled automatic update index database"                                  ),
  CMD_OPTION_SPECIAL      ("index-database-max-band-width",0,  1,1,&globalOptions.indexDatabaseMaxBandWidthList,    cmdOptionParseBandWidth,NULL,                                "max. band width to use for index updates [bis/s]","number or file name"   ),
  CMD_OPTION_INTEGER      ("index-database-keep-time",     0,  1,1,globalOptions.indexDatabaseKeepTime,             0,MAX_INT,COMMAND_LINE_TIME_UNITS,                           "time to keep index data of not existing storages",NULL                    ),

  CMD_OPTION_SET          ("log",                          0,  1,1,logTypes,                                        COMMAND_LINE_OPTIONS_LOG_TYPES,                              "log types"                                                                ),
  CMD_OPTION_CSTRING      ("log-file",                     0,  1,1,logFileName,                                                                                                  "log file name","file name"                                                ),
  CMD_OPTION_CSTRING      ("log-format",                   0,  1,1,logFormat,                                                                                                    "log format","format"                                                      ),
  CMD_OPTION_CSTRING      ("log-post-command",             0,  1,1,logPostCommand,                                                                                               "log file post-process command","command"                                  ),

  CMD_OPTION_CSTRING      ("pid-file",                     0,  1,1,pidFileName,                                                                                                  "process id file name","file name"                                         ),

  CMD_OPTION_BOOLEAN      ("info",                         0  ,0,1,globalOptions.metaInfoFlag,                                                                                   "show meta info"                                                           ),

  CMD_OPTION_BOOLEAN      ("group",                        'g',0,1,globalOptions.groupFlag,                                                                                      "group files in list"                                                      ),
  CMD_OPTION_BOOLEAN      ("all",                          0,  0,1,globalOptions.allFlag,                                                                                        "show all files"                                                           ),
  CMD_OPTION_BOOLEAN      ("long-format",                  'L',0,1,globalOptions.longFormatFlag,                                                                                 "list in long format"                                                      ),
  CMD_OPTION_BOOLEAN      ("human-format",                 'H',0,1,globalOptions.humanFormatFlag,                                                                                "list in human readable format"                                            ),
  CMD_OPTION_BOOLEAN      ("numeric-uid-gid",              0,  0,1,globalOptions.numericUIDGIDFlag,                                                                              "print numeric user/group ids"                                             ),
  CMD_OPTION_BOOLEAN      ("numeric-permission",           0,  0,1,globalOptions.numericPermissionFlag,                                                                          "print numeric file/directory permissions"                                 ),
  CMD_OPTION_BOOLEAN      ("no-header-footer",             0,  0,1,globalOptions.noHeaderFooterFlag,                                                                             "no header/footer output in list"                                          ),
  CMD_OPTION_BOOLEAN      ("delete-old-archive-files",     0,  1,1,globalOptions.deleteOldArchiveFilesFlag,                                                                      "delete old archive files after creating new files"                        ),
  CMD_OPTION_BOOLEAN      ("ignore-no-backup-file",        0,  1,2,globalOptions.ignoreNoBackupFileFlag,                                                                         "ignore .nobackup/.NOBACKUP file"                                          ),
  CMD_OPTION_BOOLEAN      ("ignore-no-dump",               0,  1,2,jobOptions.ignoreNoDumpAttributeFlag,                                                                         "ignore 'no dump' attribute of files"                                      ),

  CMD_OPTION_BOOLEAN      ("skip-unreadable",              0,  0,2,jobOptions.skipUnreadableFlag,                                                                                "skip unreadable files"                                                    ),
  CMD_OPTION_BOOLEAN      ("force-delta-compression",      0,  0,2,jobOptions.forceDeltaCompressionFlag,                                                                         "force delta compression of files. Stop on error"                          ),
  CMD_OPTION_BOOLEAN      ("raw-images",                   0,  1,2,jobOptions.rawImagesFlag,                                                                                     "store raw images (store all image blocks)"                                ),
  CMD_OPTION_BOOLEAN      ("no-fragments-check",           0,  1,2,jobOptions.noFragmentsCheckFlag,                                                                              "do not check completeness of file fragments"                              ),
  CMD_OPTION_BOOLEAN      ("no-index-database",            0,  1,1,jobOptions.noIndexDatabaseFlag,                                                                               "do not store index database for archives"                                 ),
  CMD_OPTION_SELECT       ("archive-file-mode",            0,  1,2,jobOptions.archiveFileMode,                      COMMAND_LINE_OPTIONS_ARCHIVE_FILE_MODES,                     "select archive files write mode"                                          ),
  // Note: shortcut for --archive-file-mode=overwrite
  CMD_OPTION_BOOLEAN      ("overwrite-archive-files",      'o',0,2,jobOptions.archiveFileModeOverwriteFlag,                                                                      "overwrite existing archive files"                                         ),
  CMD_OPTION_BOOLEAN      ("overwrite-files",              0,  0,2,jobOptions.overwriteEntriesFlag,                                                                              "overwrite existing entries"                                               ),
  CMD_OPTION_BOOLEAN      ("wait-first-volume",            0,  1,2,jobOptions.waitFirstVolumeFlag,                                                                               "wait for first volume"                                                    ),
  CMD_OPTION_BOOLEAN      ("dry-run",                      0,  1,2,jobOptions.dryRunFlag,                                                                                        "do dry-run (skip storage/restore, incremental data, index database)"      ),
  CMD_OPTION_BOOLEAN      ("no-signature",                 0  ,1,2,globalOptions.noSignatureFlag,                                                                                "do not create signatures"                                                 ),
  CMD_OPTION_BOOLEAN      ("skip-verify-signatures",       0,  0,2,jobOptions.skipVerifySignaturesFlag,                                                                            "do not verify signatures of archives"                                     ),
  CMD_OPTION_BOOLEAN      ("no-storage",                   0,  1,2,jobOptions.noStorageFlag,                                                                                     "do not store archives (skip storage, index database)"                     ),
  CMD_OPTION_BOOLEAN      ("no-bar-on-medium",             0,  1,2,jobOptions.noBAROnMediumFlag,                                                                                 "do not store a copy of BAR on medium"                                     ),
  CMD_OPTION_BOOLEAN      ("no-stop-on-error",             0,  1,2,jobOptions.noStopOnErrorFlag,                                                                                 "do not immediately stop on error"                                         ),

  CMD_OPTION_BOOLEAN      ("no-default-config",            0,  1,1,globalOptions.noDefaultConfigFlag,                                                                            "do not read configuration files " CONFIG_DIR "/bar.cfg and ~/.bar/" DEFAULT_CONFIG_FILE_NAME),
  CMD_OPTION_BOOLEAN      ("quiet",                        0,  1,1,globalOptions.quietFlag,                                                                                      "suppress any output"                                                      ),
  CMD_OPTION_INTEGER_RANGE("verbose",                      'v',3,1,globalOptions.verboseLevel,                      0,6,NULL,                                                    "verbosity level",NULL                                                     ),

  CMD_OPTION_BOOLEAN      ("server-debug",                 0,  2,1,globalOptions.serverDebugFlag,                                                                                "enable debug mode for server"                                             ),

  CMD_OPTION_BOOLEAN      ("version",                      0  ,0,0,versionFlag,                                                                                                  "output version"                                                           ),
  CMD_OPTION_BOOLEAN      ("help",                         'h',0,0,helpFlag,                                                                                                     "output this help"                                                         ),
  CMD_OPTION_BOOLEAN      ("xhelp",                        0,  0,0,xhelpFlag,                                                                                                    "output help to extended options"                                          ),
  CMD_OPTION_BOOLEAN      ("help-internal",                0,  1,0,helpInternalFlag,                                                                                             "output help to internal options"                                          ),

  // deprecated
  CMD_OPTION_DEPRECATED   ("mount-device",                 0,  1,2,&mountList,                                      cmdOptionParseDeprecatedMountDevice,NULL,                    "device to mount/unmount"                                                  ),
  CMD_OPTION_DEPRECATED   ("stop-on-error",                0,  1,2,&jobOptions.noStopOnErrorFlag,                   cmdOptionParseDeprecatedStopOnError,NULL,                    "no-stop-on-error"                                                         ),
};

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

// handle deprecated configuration values

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

const ConfigValueSelect CONFIG_VALUE_COMPRESS_ALGORITHMS[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"none", COMPRESS_ALGORITHM_NONE,  },

  {"zip0", COMPRESS_ALGORITHM_ZIP_0, },
  {"zip1", COMPRESS_ALGORITHM_ZIP_1, },
  {"zip2", COMPRESS_ALGORITHM_ZIP_2, },
  {"zip3", COMPRESS_ALGORITHM_ZIP_3, },
  {"zip4", COMPRESS_ALGORITHM_ZIP_4, },
  {"zip5", COMPRESS_ALGORITHM_ZIP_5, },
  {"zip6", COMPRESS_ALGORITHM_ZIP_6, },
  {"zip7", COMPRESS_ALGORITHM_ZIP_7, },
  {"zip8", COMPRESS_ALGORITHM_ZIP_8, },
  {"zip9", COMPRESS_ALGORITHM_ZIP_9, },

  #ifdef HAVE_BZ2
    {"bzip1",COMPRESS_ALGORITHM_BZIP2_1},
    {"bzip2",COMPRESS_ALGORITHM_BZIP2_2},
    {"bzip3",COMPRESS_ALGORITHM_BZIP2_3},
    {"bzip4",COMPRESS_ALGORITHM_BZIP2_4},
    {"bzip5",COMPRESS_ALGORITHM_BZIP2_5},
    {"bzip6",COMPRESS_ALGORITHM_BZIP2_6},
    {"bzip7",COMPRESS_ALGORITHM_BZIP2_7},
    {"bzip8",COMPRESS_ALGORITHM_BZIP2_8},
    {"bzip9",COMPRESS_ALGORITHM_BZIP2_9},
  #endif /* HAVE_BZ2 */

  #ifdef HAVE_LZMA
    {"lzma1",COMPRESS_ALGORITHM_LZMA_1},
    {"lzma2",COMPRESS_ALGORITHM_LZMA_2},
    {"lzma3",COMPRESS_ALGORITHM_LZMA_3},
    {"lzma4",COMPRESS_ALGORITHM_LZMA_4},
    {"lzma5",COMPRESS_ALGORITHM_LZMA_5},
    {"lzma6",COMPRESS_ALGORITHM_LZMA_6},
    {"lzma7",COMPRESS_ALGORITHM_LZMA_7},
    {"lzma8",COMPRESS_ALGORITHM_LZMA_8},
    {"lzma9",COMPRESS_ALGORITHM_LZMA_9},
  #endif /* HAVE_LZMA */

  #ifdef HAVE_XDELTA3
    {"xdelta1",COMPRESS_ALGORITHM_XDELTA_1},
    {"xdelta2",COMPRESS_ALGORITHM_XDELTA_2},
    {"xdelta3",COMPRESS_ALGORITHM_XDELTA_3},
    {"xdelta4",COMPRESS_ALGORITHM_XDELTA_4},
    {"xdelta5",COMPRESS_ALGORITHM_XDELTA_5},
    {"xdelta6",COMPRESS_ALGORITHM_XDELTA_6},
    {"xdelta7",COMPRESS_ALGORITHM_XDELTA_7},
    {"xdelta8",COMPRESS_ALGORITHM_XDELTA_8},
    {"xdelta9",COMPRESS_ALGORITHM_XDELTA_9},
  #endif /* HAVE_XDELTA3 */
);

const ConfigValueSelect CONFIG_VALUE_CRYPT_ALGORITHMS[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"none",CRYPT_ALGORITHM_NONE},

  #ifdef HAVE_GCRYPT
    {"3DES",       CRYPT_ALGORITHM_3DES       },
    {"CAST5",      CRYPT_ALGORITHM_CAST5      },
    {"BLOWFISH",   CRYPT_ALGORITHM_BLOWFISH   },
    {"AES128",     CRYPT_ALGORITHM_AES128     },
    {"AES192",     CRYPT_ALGORITHM_AES192     },
    {"AES256",     CRYPT_ALGORITHM_AES256     },
    {"TWOFISH128", CRYPT_ALGORITHM_TWOFISH128 },
    {"TWOFISH256", CRYPT_ALGORITHM_TWOFISH256 },
    {"SERPENT128", CRYPT_ALGORITHM_SERPENT128 },
    {"SERPENT192", CRYPT_ALGORITHM_SERPENT192 },
    {"SERPENT256", CRYPT_ALGORITHM_SERPENT256 },
    {"CAMELLIA128",CRYPT_ALGORITHM_CAMELLIA128},
    {"CAMELLIA192",CRYPT_ALGORITHM_CAMELLIA192},
    {"CAMELLIA256",CRYPT_ALGORITHM_CAMELLIA256},
  #endif /* HAVE_GCRYPT */
);

const ConfigValueSelect CONFIG_VALUE_CRYPT_TYPES[] = CONFIG_VALUE_SELECT_ARRAY
(
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

ConfigValue CONFIG_VALUES[] = CONFIG_VALUE_ARRAY
(
  // general settings
  CONFIG_VALUE_SPECIAL           ("config",                       &configFileNameList,-1,                                        configValueParseConfigFile,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_STRING            ("tmp-directory",                &globalOptions.tmpDirectory,-1                                 ),
  CONFIG_VALUE_INTEGER64         ("max-tmp-size",                 &globalOptions.maxTmpSize,-1,                                  0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER           ("nice-level",                   &globalOptions.niceLevel,-1,                                   0,19,NULL),
  CONFIG_VALUE_INTEGER           ("max-threads",                  &globalOptions.maxThreads,-1,                                  0,65535,NULL),

  CONFIG_VALUE_SPECIAL           ("max-band-width",               &globalOptions.maxBandWidthList,-1,                            configValueParseBandWidth,NULL,NULL,NULL,&globalOptions.maxBandWidthList),

  CONFIG_VALUE_INTEGER           ("compress-min-size",            &globalOptions.compressMinFileSize,-1,                         0,MAX_INT,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL           ("compress-exclude",             &compressExcludePatternList,-1,                                configValueParsePattern,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_CSTRING           ("continuous-database",          &continuousDatabaseFileName,-1                                 ),
  CONFIG_VALUE_INTEGER64         ("continuous-max-size",          &globalOptions.continuousMaxSize,-1,                           0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_CSTRING           ("index-database",               &indexDatabaseFileName,-1                                      ),
  CONFIG_VALUE_BOOLEAN           ("index-database-auto-update",   &globalOptions.indexDatabaseAutoUpdateFlag,-1                  ),
  CONFIG_VALUE_SPECIAL           ("index-database-max-band-width",&globalOptions.indexDatabaseMaxBandWidthList,-1,               configValueParseBandWidth,NULL,NULL,NULL,&globalOptions.indexDatabaseMaxBandWidthList),
  CONFIG_VALUE_INTEGER           ("index-database-keep-time",     &globalOptions.indexDatabaseKeepTime,-1,                       0,MAX_INT,CONFIG_VALUE_TIME_UNITS),

  // global job settings
  CONFIG_VALUE_STRING            ("UUID",                         &uuid,-1                                                       ),
  CONFIG_VALUE_IGNORE            ("host-name"),
  CONFIG_VALUE_IGNORE            ("host-port"),
  CONFIG_VALUE_STRING            ("archive-name",                 &storageName,-1                                                ),
  CONFIG_VALUE_SELECT            ("archive-type",                 &jobOptions.archiveType,-1,                                    CONFIG_VALUE_ARCHIVE_TYPES),

  CONFIG_VALUE_STRING            ("incremental-list-file",        &jobOptions.incrementalListFileName,-1                         ),

  CONFIG_VALUE_INTEGER64         ("archive-part-size",            &jobOptions.archivePartSize,-1,                                0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER           ("directory-strip",              &jobOptions.directoryStripCount,-1,                            -1,MAX_INT,NULL),
  CONFIG_VALUE_STRING            ("destination",                  &jobOptions.destination,-1                                     ),
  CONFIG_VALUE_SPECIAL           ("owner",                        &jobOptions.owner,-1,                                          configValueParseOwner,NULL,NULL,NULL,&jobOptions),

  CONFIG_VALUE_SELECT            ("pattern-type",                 &jobOptions.patternType,-1,                                    CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_VALUE_SPECIAL           ("compress-algorithm",           &jobOptions.compressAlgorithms,-1,                             configValueParseCompressAlgorithms,NULL,NULL,NULL,&jobOptions),

// multi crypt
//  CONFIG_VALUE_SPECIAL           ("crypt-algorithm",              jobOptions.cryptAlgorithms,-1,                                 configValueParseCryptAlgorithms,configValueFormatInitCryptAlgorithms,configValueFormatDoneCryptAlgorithms,configValueFormatCryptAlgorithms,NULL),
  CONFIG_VALUE_SELECT            ("crypt-algorithm",              jobOptions.cryptAlgorithms,-1,                                 CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_VALUE_SELECT            ("crypt-type",                   &jobOptions.cryptType,-1,                                      CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_VALUE_SELECT            ("crypt-password-mode",          &jobOptions.cryptPasswordMode,-1,                              CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_VALUE_SPECIAL           ("crypt-password",               &globalOptions.cryptPassword,-1,                               configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_SPECIAL           ("crypt-public-key",             &jobOptions.cryptPublicKey,-1,                                 configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("crypt-private-key",            &jobOptions.cryptPrivateKey,-1,                                configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("signature-public-key",         &globalOptions.signaturePublicKey,-1,                          configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("signature-private-key",        &globalOptions.signaturePrivateKey,-1,                         configValueParseKeyData,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_SPECIAL           ("include-file",                 &includeEntryList,-1,                                          configValueParseFileEntryPattern,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_CSTRING           ("include-file-command",         &includeFileCommand,-1                                         ),
  CONFIG_VALUE_SPECIAL           ("include-image",                &includeEntryList,-1,                                          configValueParseImageEntryPattern,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_CSTRING           ("include-image-command",        &includeImageCommand,-1                                        ),
  CONFIG_VALUE_SPECIAL           ("exclude",                      &excludePatternList,-1,                                        configValueParsePattern,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_CSTRING           ("exclude-command",              &excludeCommand,-1                                             ),
  CONFIG_VALUE_SPECIAL           ("mount",                        &mountList,-1,                                                 configValueParseMount,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_SPECIAL           ("delta-source",                 &deltaSourceList,-1,                                           configValueParseDeltaSource,NULL,NULL,NULL,&jobOptions.patternType),

  CONFIG_VALUE_INTEGER64         ("max-storage-size",             &jobOptions.maxStorageSize,-1,                                 0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
//TODO
#if 0
  CONFIG_VALUE_INTEGER           ("min-keep",                     &jobOptions.minKeep,-1,                                        0,MAX_INT,NULL),
  CONFIG_VALUE_INTEGER           ("max-keep",                     &jobOptions.maxKeep,-1,                                        0,MAX_INT,NULL),
  CONFIG_VALUE_INTEGER           ("max-age",                      &jobOptions.maxAge,-1,                                         0,MAX_INT,NULL),
#endif
  CONFIG_VALUE_INTEGER64         ("volume-size",                  &jobOptions.volumeSize,-1,                                     0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_BOOLEAN           ("ecc",                          &jobOptions.errorCorrectionCodesFlag,-1                        ),
  CONFIG_VALUE_BOOLEAN           ("always-create-image",          &jobOptions.alwaysCreateImageFlag,-1                           ),
  CONFIG_VALUE_BOOLEAN           ("blank",                        &jobOptions.blankFlag,-1                                       ),

  CONFIG_VALUE_STRING            ("comment",                      &jobOptions.comment,-1                                         ),

  CONFIG_VALUE_BOOLEAN           ("skip-unreadable",              &jobOptions.skipUnreadableFlag,-1                              ),
  CONFIG_VALUE_BOOLEAN           ("raw-images",                   &jobOptions.rawImagesFlag,-1                                   ),
  CONFIG_VALUE_BOOLEAN           ("no-fragments-check",           &jobOptions.noFragmentsCheckFlag,-1                            ),
  CONFIG_VALUE_SELECT            ("archive-file-mode",            &jobOptions.archiveFileMode,-1,                                CONFIG_VALUE_ARCHIVE_FILE_MODES),
  CONFIG_VALUE_BOOLEAN           ("overwrite-files",              &jobOptions.overwriteEntriesFlag,-1                            ),
  CONFIG_VALUE_BOOLEAN           ("wait-first-volume",            &jobOptions.waitFirstVolumeFlag,-1                             ),
  CONFIG_VALUE_BOOLEAN           ("no-signature",                 &globalOptions.noSignatureFlag,-1                              ),
  CONFIG_VALUE_BOOLEAN           ("skip-verify-signatures",       &jobOptions.skipVerifySignaturesFlag,-1                          ),
  CONFIG_VALUE_BOOLEAN           ("no-bar-on-medium",             &jobOptions.noBAROnMediumFlag,-1                               ),
  CONFIG_VALUE_BOOLEAN           ("no-stop-on-error",             &jobOptions.noStopOnErrorFlag,-1                               ),
  CONFIG_VALUE_BOOLEAN           ("quiet",                        &globalOptions.quietFlag,-1                                    ),
  CONFIG_VALUE_INTEGER           ("verbose",                      &globalOptions.verboseLevel,-1,                                0,6,NULL),

  // ignored schedule settings (server only)
  CONFIG_VALUE_BEGIN_SECTION     ("schedule",-1),
    CONFIG_VALUE_IGNORE          ("UUID"),
    CONFIG_VALUE_IGNORE          ("parentUUID"),
    CONFIG_VALUE_IGNORE          ("date"),
    CONFIG_VALUE_IGNORE          ("weekdays"),
    CONFIG_VALUE_IGNORE          ("time"),
    CONFIG_VALUE_IGNORE          ("archive-type"),
    CONFIG_VALUE_IGNORE          ("interval"),
    CONFIG_VALUE_IGNORE          ("text"),
    CONFIG_VALUE_IGNORE          ("min-keep"),
    CONFIG_VALUE_IGNORE          ("max-keep"),
    CONFIG_VALUE_IGNORE          ("max-age"),
    CONFIG_VALUE_IGNORE          ("enabled"),
  CONFIG_VALUE_END_SECTION(),

  // commands
  CONFIG_VALUE_STRING            ("pre-command",                  &jobOptions.preProcessScript,-1                                ),
  CONFIG_VALUE_STRING            ("post-command",                 &jobOptions.postProcessScript,-1                               ),

  CONFIG_VALUE_STRING            ("file-write-pre-command",       &globalOptions.file.writePreProcessCommand,-1                  ),
  CONFIG_VALUE_STRING            ("file-write-post-command",      &globalOptions.file.writePostProcessCommand,-1                 ),

  CONFIG_VALUE_STRING            ("ftp-write-pre-command",        &globalOptions.ftp.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("ftp-write-post-command",       &globalOptions.ftp.writePostProcessCommand,-1                  ),

  CONFIG_VALUE_STRING            ("scp-write-pre-command",        &globalOptions.scp.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("scp-write-post-command",       &globalOptions.scp.writePostProcessCommand,-1                  ),

  CONFIG_VALUE_STRING            ("sftp-write-pre-command",       &globalOptions.sftp.writePreProcessCommand,-1                  ),
  CONFIG_VALUE_STRING            ("sftp-write-post-command",      &globalOptions.sftp.writePostProcessCommand,-1                 ),

  CONFIG_VALUE_STRING            ("webdav-write-pre-command",     &globalOptions.webdav.writePreProcessCommand,-1                ),
  CONFIG_VALUE_STRING            ("webdav-write-post-command",    &globalOptions.webdav.writePostProcessCommand,-1               ),

  CONFIG_VALUE_STRING            ("cd-device",                    &globalOptions.cd.deviceName,-1                                ),
  CONFIG_VALUE_STRING            ("cd-request-volume-command",    &globalOptions.cd.requestVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING            ("cd-unload-volume-command",     &globalOptions.cd.unloadVolumeCommand,-1                       ),
  CONFIG_VALUE_STRING            ("cd-load-volume-command",       &globalOptions.cd.loadVolumeCommand,-1                         ),
  CONFIG_VALUE_INTEGER64         ("cd-volume-size",               &globalOptions.cd.volumeSize,-1,                               0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING            ("cd-image-pre-command",         &globalOptions.cd.imagePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("cd-image-post-command",        &globalOptions.cd.imagePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("cd-image-command",             &globalOptions.cd.imageCommand,-1                              ),
  CONFIG_VALUE_STRING            ("cd-ecc-pre-command",           &globalOptions.cd.eccPreProcessCommand,-1                      ),
  CONFIG_VALUE_STRING            ("cd-ecc-post-command",          &globalOptions.cd.eccPostProcessCommand,-1                     ),
  CONFIG_VALUE_STRING            ("cd-ecc-command",               &globalOptions.cd.eccCommand,-1                                ),
  CONFIG_VALUE_STRING            ("cd-blank-command",             &globalOptions.cd.blankCommand,-1                              ),
  CONFIG_VALUE_STRING            ("cd-write-pre-command",         &globalOptions.cd.writePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("cd-write-post-command",        &globalOptions.cd.writePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("cd-write-command",             &globalOptions.cd.writeCommand,-1                              ),
  CONFIG_VALUE_STRING            ("cd-write-image-command",       &globalOptions.cd.writeImageCommand,-1                         ),

  CONFIG_VALUE_STRING            ("dvd-device",                   &globalOptions.dvd.deviceName,-1                               ),
  CONFIG_VALUE_STRING            ("dvd-request-volume-command",   &globalOptions.dvd.requestVolumeCommand,-1                     ),
  CONFIG_VALUE_STRING            ("dvd-unload-volume-command",    &globalOptions.dvd.unloadVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING            ("dvd-load-volume-command",      &globalOptions.dvd.loadVolumeCommand,-1                        ),
  CONFIG_VALUE_INTEGER64         ("dvd-volume-size",              &globalOptions.dvd.volumeSize,-1,                              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING            ("dvd-image-pre-command",        &globalOptions.dvd.imagePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("dvd-image-post-command",       &globalOptions.dvd.imagePostProcessCommand,-1                  ),
  CONFIG_VALUE_STRING            ("dvd-image-command",            &globalOptions.dvd.imageCommand,-1                             ),
  CONFIG_VALUE_STRING            ("dvd-ecc-pre-command",          &globalOptions.dvd.eccPreProcessCommand,-1                     ),
  CONFIG_VALUE_STRING            ("dvd-ecc-post-command",         &globalOptions.dvd.eccPostProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("dvd-ecc-command",              &globalOptions.dvd.eccCommand,-1                               ),
  CONFIG_VALUE_STRING            ("dvd-blank-command",            &globalOptions.dvd.blankCommand,-1                             ),
  CONFIG_VALUE_STRING            ("dvd-write-pre-command",        &globalOptions.dvd.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("dvd-write-post-command",       &globalOptions.dvd.writePostProcessCommand,-1                  ),
  CONFIG_VALUE_STRING            ("dvd-write-command",            &globalOptions.dvd.writeCommand,-1                             ),
  CONFIG_VALUE_STRING            ("dvd-write-image-command",      &globalOptions.dvd.writeImageCommand,-1                        ),

  CONFIG_VALUE_STRING            ("bd-device",                    &globalOptions.bd.deviceName,-1                                ),
  CONFIG_VALUE_STRING            ("bd-request-volume-command",    &globalOptions.bd.requestVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING            ("bd-unload-volume-command",     &globalOptions.bd.unloadVolumeCommand,-1                       ),
  CONFIG_VALUE_STRING            ("bd-load-volume-command",       &globalOptions.bd.loadVolumeCommand,-1                         ),
  CONFIG_VALUE_INTEGER64         ("bd-volume-size",               &globalOptions.bd.volumeSize,-1,                               0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING            ("bd-image-pre-command",         &globalOptions.bd.imagePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("bd-image-post-command",        &globalOptions.bd.imagePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("bd-image-command",             &globalOptions.bd.imageCommand,-1                              ),
  CONFIG_VALUE_STRING            ("bd-ecc-pre-command",           &globalOptions.bd.eccPreProcessCommand,-1                      ),
  CONFIG_VALUE_STRING            ("bd-ecc-post-command",          &globalOptions.bd.eccPostProcessCommand,-1                     ),
  CONFIG_VALUE_STRING            ("bd-ecc-command",               &globalOptions.bd.eccCommand,-1                                ),
  CONFIG_VALUE_STRING            ("bd-blank-command",             &globalOptions.bd.blankCommand,-1                              ),
  CONFIG_VALUE_STRING            ("bd-write-pre-command",         &globalOptions.bd.writePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING            ("bd-write-post-command",        &globalOptions.bd.writePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING            ("bd-write-command",             &globalOptions.bd.writeCommand,-1                              ),
  CONFIG_VALUE_STRING            ("bd-write-image-command",       &globalOptions.bd.writeImageCommand,-1                         ),

//  CONFIG_VALUE_INTEGER64         ("file-max-storage-size",        &defaultFileServer.maxStorageSize,-1,                           0LL,MAX_INT64,NULL),
  CONFIG_VALUE_BEGIN_SECTION     ("file-server",-1),
    CONFIG_STRUCT_VALUE_INTEGER64("file-max-storage-size",        Server,maxStorageSize,                                         0LL,MAX_INT64,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("file-write-pre-command",       Server,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("file-write-post-command",      Server,writePostProcessCommand                                 ),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_STRING            ("ftp-login-name",               &defaultFTPServer.ftp.loginName,-1                             ),
  CONFIG_VALUE_SPECIAL           ("ftp-password",                 &defaultFTPServer.ftp.password,-1,                             configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_INTEGER           ("ftp-max-connections",          &defaultFTPServer.maxConnectionCount,-1,                       0,MAX_INT,NULL),
  CONFIG_VALUE_INTEGER64         ("ftp-max-storage-size",         &defaultFTPServer.maxStorageSize,-1,                           0LL,MAX_INT64,NULL),
  CONFIG_VALUE_BEGIN_SECTION     ("ftp-server",-1),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-login-name",               Server,ftp.loginName                                           ),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ftp-password",                 Server,ftp.password,                                           configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("ftp-max-connections",          Server,maxConnectionCount,                                     0,MAX_INT,NULL),
    CONFIG_STRUCT_VALUE_INTEGER64("ftp-max-storage-size",         Server,maxStorageSize,                                         0LL,MAX_INT64,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-write-pre-command",        Server,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("ftp-write-post-command",       Server,writePostProcessCommand                                 ),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_INTEGER           ("ssh-port",                     &defaultSSHServer.ssh.port,-1,                                 0,65535,NULL),
  CONFIG_VALUE_STRING            ("ssh-login-name",               &defaultSSHServer.ssh.loginName,-1                             ),
  CONFIG_VALUE_SPECIAL           ("ssh-password",                 &defaultSSHServer.ssh.password,-1,                             configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_SPECIAL           ("ssh-public-key",               &defaultSSHServer.ssh.publicKey,-1,                            configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("ssh-private-key",              &defaultSSHServer.ssh.privateKey,-1,                           configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_INTEGER           ("ssh-max-connections",          &defaultSSHServer.maxConnectionCount,-1,                       0,MAX_INT,NULL),
  CONFIG_VALUE_INTEGER64         ("ssh-max-storage-size",         &defaultSSHServer.maxStorageSize,-1,                           0LL,MAX_INT64,NULL),
  CONFIG_VALUE_BEGIN_SECTION     ("ssh-server",-1),
    CONFIG_STRUCT_VALUE_INTEGER  ("ssh-port",                     Server,ssh.port,                                               0,65535,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-login-name",               Server,ssh.loginName                                           ),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-password",                 Server,ssh.password,                                           configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-public-key",               Server,ssh.publicKey,                                          configValueParseKeyData,configValueFormatInitKeyData,configValueFormatDoneKeyData,configValueFormatKeyData,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-private-key",              Server,ssh.privateKey,                                         configValueParseKeyData,configValueFormatInitKeyData,configValueFormatDoneKeyData,configValueFormatKeyData,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("ssh-max-connections",          Server,maxConnectionCount,                                     0,MAX_INT,NULL),
    CONFIG_STRUCT_VALUE_INTEGER64("ssh-max-storage-size",         Server,maxStorageSize,                                         0LL,MAX_INT64,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-write-pre-command",        Server,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("ssh-write-post-command",       Server,writePostProcessCommand                                 ),
  CONFIG_VALUE_END_SECTION(),

//  CONFIG_VALUE_INTEGER           ("webdav-port",                  &defaultWebDAVServer.webDAV.port,-1,                           0,65535,NULL),
  CONFIG_VALUE_STRING            ("webdav-login-name",            &defaultWebDAVServer.webDAV.loginName,-1                       ),
  CONFIG_VALUE_SPECIAL           ("webdav-password",              &defaultWebDAVServer.webDAV.password,-1,                       configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_INTEGER           ("webdav-max-connections",       &defaultWebDAVServer.maxConnectionCount,-1,                    0,MAX_INT,NULL),
  CONFIG_VALUE_INTEGER64         ("webdav-max-storage-size",      &defaultWebDAVServer.maxStorageSize,-1,                        0LL,MAX_INT64,NULL),
  CONFIG_VALUE_BEGIN_SECTION     ("webdav-server",-1),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-login-name",            Server,webDAV.loginName                                        ),
    CONFIG_STRUCT_VALUE_SPECIAL  ("webdav-password",              Server,webDAV.password,                                        configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
    CONFIG_STRUCT_VALUE_INTEGER  ("webdav-max-connections",       Server,maxConnectionCount,                                     0,MAX_INT,NULL),
    CONFIG_STRUCT_VALUE_INTEGER64("webdav-max-storage-size",      Server,maxStorageSize,                                         0LL,MAX_INT64,NULL),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-write-pre-command",     Server,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("webdav-write-post-command",    Server,writePostProcessCommand                                 ),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_STRING            ("device",                       &defaultDevice.name,-1                                         ),
  CONFIG_VALUE_STRING            ("device-request-volume-command",&defaultDevice.requestVolumeCommand,-1                         ),
  CONFIG_VALUE_STRING            ("device-unload-volume-command", &defaultDevice.unloadVolumeCommand,-1                          ),
  CONFIG_VALUE_STRING            ("device-load-volume-command",   &defaultDevice.loadVolumeCommand,-1                            ),
  CONFIG_VALUE_INTEGER64         ("device-volume-size",           &defaultDevice.volumeSize,-1,                                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING            ("device-image-pre-command",     &defaultDevice.imagePreProcessCommand,-1                       ),
  CONFIG_VALUE_STRING            ("device-image-post-command",    &defaultDevice.imagePostProcessCommand,-1                      ),
  CONFIG_VALUE_STRING            ("device-image-command",         &defaultDevice.imageCommand,-1                                 ),
  CONFIG_VALUE_STRING            ("device-ecc-pre-command",       &defaultDevice.eccPreProcessCommand,-1                         ),
  CONFIG_VALUE_STRING            ("device-ecc-post-command",      &defaultDevice.eccPostProcessCommand,-1                        ),
  CONFIG_VALUE_STRING            ("device-ecc-command",           &defaultDevice.eccCommand,-1                                   ),
  CONFIG_VALUE_STRING            ("device-blank-command",         &defaultDevice.blankCommand,-1                                 ),
  CONFIG_VALUE_STRING            ("device-write-pre-command",     &defaultDevice.writePreProcessCommand,-1                       ),
  CONFIG_VALUE_STRING            ("device-write-post-command",    &defaultDevice.writePostProcessCommand,-1                      ),
  CONFIG_VALUE_STRING            ("device-write-command",         &defaultDevice.writeCommand,-1                                 ),
//  CONFIG_VALUE_BEGIN_SECTION("device",-1),
  CONFIG_VALUE_SECTION_ARRAY("device",-1,
    CONFIG_STRUCT_VALUE_STRING   ("device-name",                  Device,name                                                    ),
    CONFIG_STRUCT_VALUE_STRING   ("device-request-volume-command",Device,requestVolumeCommand                                    ),
    CONFIG_STRUCT_VALUE_STRING   ("device-unload-volume-command", Device,unloadVolumeCommand                                     ),
    CONFIG_STRUCT_VALUE_STRING   ("device-load-volume-command",   Device,loadVolumeCommand                                       ),
    CONFIG_STRUCT_VALUE_INTEGER64("device-volume-size",           Device,volumeSize,                                             0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-pre-command",     Device,imagePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-post-command",    Device,imagePostProcessCommand                                 ),
    CONFIG_STRUCT_VALUE_STRING   ("device-image-command",         Device,imageCommand                                            ),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-pre-command",       Device,eccPreProcessCommand                                    ),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-post-command",      Device,eccPostProcessCommand                                   ),
    CONFIG_STRUCT_VALUE_STRING   ("device-ecc-command",           Device,eccCommand                                              ),
    CONFIG_STRUCT_VALUE_STRING   ("device-blank-command",         Device,blankCommand                                            ),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-pre-command",     Device,writePreProcessCommand                                  ),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-post-command",    Device,writePostProcessCommand                                 ),
    CONFIG_STRUCT_VALUE_STRING   ("device-write-command",         Device,writeCommand                                            ),
  ),
//  CONFIG_VALUE_END_SECTION(),

  // server settings
  CONFIG_VALUE_INTEGER           ("server-port",                  &serverPort,-1,                                                0,65535,NULL),
  CONFIG_VALUE_INTEGER           ("server-tls-port",              &serverTLSPort,-1,                                             0,65535,NULL),
  CONFIG_VALUE_SPECIAL           ("server-ca-file",               &serverCA,-1,                                                  configValueParseCertificate,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("server-cert-file",             &serverCert,-1,                                                configValueParseCertificate,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("server-key-file",              &serverKey,-1,                                                 configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL           ("server-password",              &serverPassword,-1,                                            configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_VALUE_INTEGER           ("server-max-connections",       &serverMaxConnections,-1,                                      0,65535,NULL),
  CONFIG_VALUE_CSTRING           ("server-jobs-directory",        &serverJobsDirectory,-1                                        ),

  CONFIG_VALUE_STRING            ("remote-bar-executable",        &globalOptions.remoteBARExecutable,-1                          ),

  CONFIG_VALUE_SET               ("log",                          &logTypes,-1,                                                  CONFIG_VALUE_LOG_TYPES),
  CONFIG_VALUE_CSTRING           ("log-file",                     &logFileName,-1                                                ),
  CONFIG_VALUE_CSTRING           ("log-format",                   &logFormat,-1                                                  ),
  CONFIG_VALUE_CSTRING           ("log-post-command",             &logPostCommand,-1                                             ),

  CONFIG_VALUE_COMMENT           ("process id file"),
  CONFIG_VALUE_CSTRING           ("pid-file",                     &pidFileName,-1                                                ),

  // deprecated
  CONFIG_VALUE_DEPRECATED        ("mount-device",                 &mountList,-1,                                                 configValueParseDeprecatedMountDevice,NULL,"mount"),
  CONFIG_VALUE_IGNORE            ("schedule"),
//TODO
  CONFIG_VALUE_IGNORE            ("overwrite-archive-files"),
  CONFIG_VALUE_DEPRECATED        ("stop-on-error",                &jobOptions.noStopOnErrorFlag,-1,                              configValueParseDeprecatedStopOnError,NULL,"no-stop-on-error"),
);

/*---------------------------------------------------------------------*/

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

LOCAL void signalHandler(int signalNumber, siginfo_t *siginfo, void *context)
{
  struct sigaction signalAction;

  UNUSED_VARIABLE(siginfo);
  UNUSED_VARIABLE(context);

  // deinstall signal handlers
  sigfillset(&signalAction.sa_mask);
  signalAction.sa_flags   = 0;
  signalAction.sa_handler = SIG_DFL;
  sigaction(SIGINT,&signalAction,NULL);
  sigaction(SIGQUIT,&signalAction,NULL);
  sigaction(SIGTERM,&signalAction,NULL);
  sigaction(SIGBUS,&signalAction,NULL);
  sigaction(SIGILL,&signalAction,NULL);
  sigaction(SIGFPE,&signalAction,NULL);
  sigaction(SIGSEGV,&signalAction,NULL);

  // delete pid file
  deletePIDFile();

  // delete temporary directory (Note: do a simple validity check in case something serious went wrong...)
  if (!String_isEmpty(tmpDirectory) && !String_equalsCString(tmpDirectory,"/"))
  {
    File_delete(tmpDirectory,TRUE);
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
            (void)fwrite("\b",1,1,file);
          }
          for (i = 0; i < String_length(lastOutputLine); i++)
          {
            (void)fwrite(" ",1,1,file);
          }
          for (i = 0; i < String_length(lastOutputLine); i++)
          {
            (void)fwrite("\b",1,1,file);
          }
          fflush(file);
        }

        // restore line
        (void)fwrite(String_cString(outputLine),1,String_length(outputLine),file);
      }

      // output new string
      (void)fwrite(String_cString(string),1,String_length(string),file);

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
        if (outputLine != NULL) (void)fwrite(String_cString(outputLine),1,String_length(outputLine),file);
        (void)fwrite(String_cString(string),1,String_length(string),file);
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
    (void)fwrite(String_cString(string),1,String_length(string),file);
  }
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

  String_delete(deviceNode->device.writeCommand           );
  String_delete(deviceNode->device.writePostProcessCommand);
  String_delete(deviceNode->device.writePreProcessCommand );
  String_delete(deviceNode->device.blankCommand           );
  String_delete(deviceNode->device.eccCommand             );
  String_delete(deviceNode->device.eccPostProcessCommand  );
  String_delete(deviceNode->device.eccPreProcessCommand   );
  String_delete(deviceNode->device.imageCommand           );
  String_delete(deviceNode->device.imagePostProcessCommand);
  String_delete(deviceNode->device.imagePreProcessCommand );
  String_delete(deviceNode->device.loadVolumeCommand      );
  String_delete(deviceNode->device.unloadVolumeCommand    );
  String_delete(deviceNode->device.requestVolumeCommand   );
  String_delete(deviceNode->device.name                   );
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
  error = File_getFileInfo(fileName,&fileInfo);
  if (error == ERROR_NONE)
  {
    if ((fileInfo.permission & (FILE_PERMISSION_GROUP_READ|FILE_PERMISSION_OTHER_READ)) != 0)
    {
      printWarning(_("Configuration file '%s' has wrong file permission %03o. Please make sure read permissions are limited to file owner (mode 600).\n"),
                   String_cString(fileName),
                   fileInfo.permission & FILE_PERMISSION_MASK
                  );
    }
  }
  else
  {
    printWarning(_("Cannot get file info for configuration file '%s' (error: %s)\n"),
                 String_cString(fileName),
                 Error_getText(error)
                );
  }

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError(_("Cannot open configuration file '%s' (error: %s)!\n"),
               String_cString(fileName),
               Error_getText(error)
              );
    return FALSE;
  }

  // parse file
  if (printInfoFlag) { printf("Reading configuration file '%s'...",String_cString(fileName)); fflush(stdout); }
  failFlag   = FALSE;
  line       = String_new();
  lineNb     = 0;
  name       = String_new();
  value      = String_new();
  while (   !failFlag
         && File_getLine(&fileHandle,line,&lineNb,"#")
        )
  {
    // parse line
    if      (String_parse(line,STRING_BEGIN,"[file-server %S]",NULL,name))
    {
      SemaphoreLock semaphoreLock;
      ServerNode    *serverNode;

      // find/allocate server node
      serverNode = NULL;
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,String_equals(serverNode->server.name,name));
        if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
      }
      if (serverNode == NULL) serverNode = newServerNode(name,SERVER_TYPE_FILE);

      // parse section
      while (   !failFlag
             && File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
            )
      {
        if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
        {
          String_unquote(value,STRING_QUOTES);
          String_unescape(value,
                          STRING_ESCAPE_CHARACTER,
                          STRING_ESCAPE_CHARACTERS_MAP_TO,
                          STRING_ESCAPE_CHARACTERS_MAP_FROM,
                          STRING_ESCAPE_CHARACTER_MAP_LENGTH
                        );
          if (!ConfigValue_parse(String_cString(name),
                                 String_cString(value),
                                 CONFIG_VALUES,
                                 "file-server",
                                 NULL, // outputHandle
                                 NULL, // errorPrefix
                                 NULL, // warningPrefix
                                 &serverNode->server
                                )
             )
          {
            if (printInfoFlag) printf("FAIL!\n");
            printError("Unknown or invalid config value '%s' in section '%s' in %s, line %ld\n",
                       String_cString(name),
                       "ftp-server",
                       String_cString(fileName),
                       lineNb
                      );
            failFlag = TRUE;
            break;
          }
        }
        else
        {
          if (printInfoFlag) printf("FAIL!\n");
          printError("Syntax error in '%s', line %ld: '%s'\n",
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
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        List_append(&globalOptions.serverList,serverNode);
      }
    }
    else if (String_parse(line,STRING_BEGIN,"[ftp-server %S]",NULL,name))
    {
      SemaphoreLock semaphoreLock;
      ServerNode    *serverNode;

      // find/allocate server node
      serverNode = NULL;
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,String_equals(serverNode->server.name,name));
        if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
      }
      if (serverNode == NULL) serverNode = newServerNode(name,SERVER_TYPE_FTP);

      // parse section
      while (   !failFlag
             && File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
            )
      {
        if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
        {
          String_unquote(value,STRING_QUOTES);
          String_unescape(value,
                          STRING_ESCAPE_CHARACTER,
                          STRING_ESCAPE_CHARACTERS_MAP_TO,
                          STRING_ESCAPE_CHARACTERS_MAP_FROM,
                          STRING_ESCAPE_CHARACTER_MAP_LENGTH
                        );
          if (!ConfigValue_parse(String_cString(name),
                                 String_cString(value),
                                 CONFIG_VALUES,
                                 "ftp-server",
                                 NULL, // outputHandle
                                 NULL, // errorPrefix
                                 NULL, // warningPrefix
                                 &serverNode->server
                                )
             )
          {
            if (printInfoFlag) printf("FAIL!\n");
            printError("Unknown or invalid config value '%s' in section '%s' in %s, line %ld\n",
                       String_cString(name),
                       "ftp-server",
                       String_cString(fileName),
                       lineNb
                      );
            failFlag = TRUE;
            break;
          }
        }
        else
        {
          if (printInfoFlag) printf("FAIL!\n");
          printError("xSyntax error in '%s', line %ld: '%s'\n",
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
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        List_append(&globalOptions.serverList,serverNode);
      }
    }
    else if (String_parse(line,STRING_BEGIN,"[ssh-server %S]",NULL,name))
    {
      SemaphoreLock semaphoreLock;
      ServerNode    *serverNode;

      // find/allocate server node
      serverNode = NULL;
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,String_equals(serverNode->server.name,name));
        if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
      }
      if (serverNode == NULL) serverNode = newServerNode(name,SERVER_TYPE_SSH);

      // parse section
      while (   !failFlag
             && File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
            )
      {
        if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
        {
          String_unquote(value,STRING_QUOTES);
          String_unescape(value,
                          STRING_ESCAPE_CHARACTER,
                          STRING_ESCAPE_CHARACTERS_MAP_TO,
                          STRING_ESCAPE_CHARACTERS_MAP_FROM,
                          STRING_ESCAPE_CHARACTER_MAP_LENGTH
                        );
          if (!ConfigValue_parse(String_cString(name),
                                 String_cString(value),
                                 CONFIG_VALUES,
                                 "ssh-server",
                                 NULL, // outputHandle
                                 NULL, // errorPrefix
                                 NULL, // warningPrefix
                                 &serverNode->server
                                )
             )
          {
            if (printInfoFlag) printf("FAIL!\n");
            printError("Unknown or invalid config value '%s' in section '%s' in %s, line %ld\n",
                       String_cString(name),
                       "ssh-server",
                       String_cString(fileName),
                       lineNb
                      );
            failFlag = TRUE;
            break;
          }
        }
        else
        {
          if (printInfoFlag) printf("FAIL!\n");
          printError("Syntax error in '%s', line %ld: '%s'\n",
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
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        List_append(&globalOptions.serverList,serverNode);
      }
    }
    else if (String_parse(line,STRING_BEGIN,"[webdav-server %S]",NULL,name))
    {
      SemaphoreLock semaphoreLock;
      ServerNode    *serverNode;

      // find/allocate server node
      serverNode = NULL;
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,String_equals(serverNode->server.name,name));
        if (serverNode != NULL) List_remove(&globalOptions.serverList,serverNode);
      }
      if (serverNode == NULL) serverNode = newServerNode(name,SERVER_TYPE_WEBDAV);

      // parse section
      while (   !failFlag
             && File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
            )
      {
        if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
        {
          String_unquote(value,STRING_QUOTES);
          String_unescape(value,
                          STRING_ESCAPE_CHARACTER,
                          STRING_ESCAPE_CHARACTERS_MAP_TO,
                          STRING_ESCAPE_CHARACTERS_MAP_FROM,
                          STRING_ESCAPE_CHARACTER_MAP_LENGTH
                        );
          if (!ConfigValue_parse(String_cString(name),
                                 String_cString(value),
                                 CONFIG_VALUES,
                                 "webdav-server",
                                 NULL, // outputHandle
                                 NULL, // errorPrefix
                                 NULL, // warningPrefix
                                 &serverNode->server
                                )
             )
          {
            if (printInfoFlag) printf("FAIL!\n");
            printError("Unknown or invalid config value '%s' in section '%s' in %s, line %ld\n",
                       String_cString(name),
                       "webdav-server",
                       String_cString(fileName),
                       lineNb
                      );
            failFlag = TRUE;
            break;
          }
        }
        else
        {
          if (printInfoFlag) printf("FAIL!\n");
          printError("Syntax error in '%s', line %ld: '%s'\n",
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
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        List_append(&globalOptions.serverList,serverNode);
      }
    }
    else if (String_parse(line,STRING_BEGIN,"[device %S]",NULL,name))
    {
      SemaphoreLock semaphoreLock;
      DeviceNode    *deviceNode;

      // find/allocate device node
      deviceNode = NULL;
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
          String_unquote(value,STRING_QUOTES);
          String_unescape(value,
                          STRING_ESCAPE_CHARACTER,
                          STRING_ESCAPE_CHARACTERS_MAP_TO,
                          STRING_ESCAPE_CHARACTERS_MAP_FROM,
                          STRING_ESCAPE_CHARACTER_MAP_LENGTH
                        );
          if (!ConfigValue_parse(String_cString(name),
                                 String_cString(value),
                                 CONFIG_VALUES,
                                 "device",
                                 NULL, // outputHandle
                                 NULL, // errorPrefix
                                 NULL, // warningPrefix
                                 &deviceNode->device
                                )
             )
          {
            if (printInfoFlag) printf("FAIL!\n");
            printError("Unknown or invalid config value '%s' in section '%s' in %s, line %ld\n",
                       String_cString(name),
                       "device",
                       String_cString(fileName),
                       lineNb
                      );
            failFlag = TRUE;
            break;
          }
        }
        else
        {
          if (printInfoFlag) printf("FAIL!\n");
          printError("Syntax error in '%s', line %ld: '%s'\n",
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
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        List_append(&globalOptions.deviceList,deviceNode);
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
      String_unquote(value,STRING_QUOTES);
      if (!ConfigValue_parse(String_cString(name),
                             String_cString(value),
                             CONFIG_VALUES,
                             NULL, // section name
                             NULL, // outputHandle,
                             NULL, // errorPrefix,
                             NULL, // warningPrefix
                             NULL  // variable
                            )
         )
      {
        if (printInfoFlag) printf("FAIL!\n");
        printError("Unknown or invalid config entry '%s' with value '%s' in %s, line %ld\n",
                   String_cString(name),
                   String_cString(value),
                   String_cString(fileName),
                   lineNb
                  );
        failFlag = TRUE;
        break;
      }
    }
    else
    {
      if (printInfoFlag) printf("FAIL!\n");
      printError("Unknown config entry '%s' in %s, line %ld\n",
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
    if (printInfoFlag) printf("OK\n");
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
#ifndef WERROR
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

LOCAL Errors readCertificateFile(Certificate *certificate, const char *fileName)
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

  return ERROR_NONE;
}

LOCAL Errors readKeyFile(Key *key, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  uint64     dataLength;
  void       *data;

  assert(key != NULL);
  assert(fileName != NULL);

  // open file
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
    return ERROR_NO_TLS_KEY;
  }

  // allocate secure memory
  data = Password_allocSecure((size_t)dataLength);
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
    Password_freeSecure(data);
    (void)File_close(&fileHandle);
    return error;
  }

  // close file
  (void)File_close(&fileHandle);

  // set key data
  if (key->data != NULL) Password_freeSecure(key->data);
  key->data   = data;
  key->length = dataLength;

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
      HALT_INTERNAL_ERROR("no valid command set");
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
    strncpy(errorMessage,Error_getText(error),errorMessageSize); errorMessage[errorMessageSize-1] = '\0';
    return FALSE;
  }

  return TRUE;
}

//TODO: remove
#if 0
/***********************************************************************\
* Name   : cmdOptionParseEntryPatternCommand
* Purpose: command line option call back for parsing include
*          patterns command
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseEntryPatternCommand(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  EntryTypes entryType;
  String     script;
//  TextMacro  textMacros[5];
  Errors     error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  // init variables
  script = String_new();

  // get entry type
  entryType = ENTRY_TYPE_FILE;
  switch (command)
  {
    case COMMAND_CREATE_FILES:
    case COMMAND_LIST:
    case COMMAND_TEST:
    case COMMAND_COMPARE:
    case COMMAND_RESTORE:
    case COMMAND_CONVERT:
    case COMMAND_INFO:
    case COMMAND_CHECK_SIGNATURE:
    case COMMAND_GENERATE_ENCRYPTION_KEYS:
    case COMMAND_GENERATE_SIGNATURE_KEYS
    case COMMAND_NEW_KEY_PASSWORD:
      entryType = ENTRY_TYPE_FILE;
      break;
    case COMMAND_CREATE_IMAGES:
      entryType = ENTRY_TYPE_IMAGE;
      break;
    default:
      HALT_INTERNAL_ERROR("no valid command set");
      break; // not reached
  }

  // expand template
//  TEXT_MACRO_N_STRING (textMacros[1],"%name",jobName,                             TEXT_MACRO_PATTERN_STRING);
//  TEXT_MACRO_N_CSTRING(textMacros[2],"%type",getArchiveTypeName(archiveType),     TEXT_MACRO_PATTERN_STRING);
//  TEXT_MACRO_N_CSTRING(textMacros[3],"%T",   getArchiveTypeShortName(archiveType),".");
//  TEXT_MACRO_N_STRING (textMacros[4],"%text",scheduleCustomText,                  TEXT_MACRO_PATTERN_STRING);
  Misc_expandMacros(script,
                    value,
                    EXPAND_MACRO_MODE_STRING,
NULL,0,//                    textMacros,SIZE_OF_ARRAY(textMacros),
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               EntryList_append((EntryList*)variable,entryType,line,PATTERN_TYPE_GLOB,NULL);
                             },NULL),
                             CALLBACK(NULL,NULL)
                            );
  if (error != ERROR_NONE)
  {
    strncpy(errorMessage,Error_getText(error),errorMessageSize); errorMessage[errorMessageSize-1] = '\0';
    String_delete(script);
    return FALSE;
  }

  // free resources
  String_delete(script);

  return TRUE;
}
#endif

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
    strncpy(errorMessage,Error_getText(error),errorMessageSize); errorMessage[errorMessageSize-1] = '\0';
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
  bool      alwaysUnmount;
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // init variables
  mountName     = String_new();
  alwaysUnmount = FALSE;

  // get name, alwaysUnmount
  if      (String_parseCString(value,"%S,%y",NULL,mountName,&alwaysUnmount))
  {
  }
  else if (String_parseCString(value,"%S",NULL,mountName))
  {
    alwaysUnmount = FALSE;
  }
  else
  {
    String_delete(mountName);
    return FALSE;
  }

  if (!String_isEmpty(mountName))
  {
    // add to mount list
    mountNode = newMountNode(mountName,
                             NULL,  // deviceName
                             alwaysUnmount
                            );
    assert(mountNode != NULL);
    List_append((MountList*)variable,mountNode);
  }

  // free resources
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
  if      (strncmp(value,"r:",2) == 0) { String_setCString(storageName, value+2); patternType = PATTERN_TYPE_REGEX;          }
  else if (strncmp(value,"x:",2) == 0) { String_setCString(storageName, value+2); patternType = PATTERN_TYPE_EXTENDED_REGEX; }
  else if (strncmp(value,"g:",2) == 0) { String_setCString(storageName, value+2); patternType = PATTERN_TYPE_GLOB;           }
  else                                 { String_setCString(storageName, value  ); patternType = PATTERN_TYPE_GLOB;           }

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
  char                          algorithm1[256],algorithm2[256];
  CompressAlgorithms            compressAlgorithmDelta,compressAlgorithmByte;
  bool                          foundFlag;
  const CommandLineOptionSelect *select;

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
    foundFlag = FALSE;
    for (select = COMPRESS_ALGORITHMS_DELTA; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(algorithm1,select->name))
      {
        compressAlgorithmDelta = (CompressAlgorithms)select->value;
        foundFlag = TRUE;
        break;
      }
    }
    for (select = COMPRESS_ALGORITHMS_BYTE; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(algorithm1,select->name))
      {
        compressAlgorithmByte = (CompressAlgorithms)select->value;
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
        compressAlgorithmDelta = (CompressAlgorithms)select->value;
        foundFlag = TRUE;
        break;
      }
    }
    for (select = COMPRESS_ALGORITHMS_BYTE; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(algorithm2,select->name))
      {
        compressAlgorithmByte = (CompressAlgorithms)select->value;
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
        compressAlgorithmDelta = (CompressAlgorithms)select->value;
        foundFlag = TRUE;
        break;
      }
    }
    for (select = COMPRESS_ALGORITHMS_BYTE; select->name != NULL; select++)
    {
      if (stringEqualsIgnoreCase(value,select->name))
      {
        compressAlgorithmByte = (CompressAlgorithms)select->value;
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
  ((JobOptionsCompressAlgorithms*)variable)->delta = compressAlgorithmDelta;
  ((JobOptionsCompressAlgorithms*)variable)->byte  = compressAlgorithmByte;

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
* Input  : s - band width string
* Output : -
* Return : band width node or NULL
* Notes  : -
\***********************************************************************/

LOCAL BandWidthNode *parseBandWidth(ConstString s)
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
    if (!parseBandWidthNumber(s0,&bandWidthNode->n)) errorFlag = TRUE;
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
    errorFlag = TRUE;
  }
  if (nextIndex < (long)String_length(s))
  {
    if      (String_parse(s,nextIndex,"%S-%S-%S",&nextIndex,s0,s1,s2))
    {
      if (!parseDateTimeNumber(s0,&bandWidthNode->year )) errorFlag = TRUE;
      if (!parseDateMonth     (s1,&bandWidthNode->month)) errorFlag = TRUE;
      if (!parseDateTimeNumber(s2,&bandWidthNode->day  )) errorFlag = TRUE;
    }
    else
    {
      errorFlag = TRUE;
    }
    if (nextIndex < (long)String_length(s))
    {
      if      (String_parse(s,nextIndex,"%S %S:%S",&nextIndex,s0,s1,s2))
      {
        if (!parseWeekDaySet(String_cString(s0),&bandWidthNode->weekDaySet)) errorFlag = TRUE;
        if (!parseDateTimeNumber(s1,&bandWidthNode->hour  )) errorFlag = TRUE;
        if (!parseDateTimeNumber(s2,&bandWidthNode->minute)) errorFlag = TRUE;
      }
      else if (String_parse(s,nextIndex,"%S:%S",&nextIndex,s0,s1))
      {
        if (!parseDateTimeNumber(s0,&bandWidthNode->hour  )) errorFlag = TRUE;
        if (!parseDateTimeNumber(s1,&bandWidthNode->minute)) errorFlag = TRUE;
      }
      else
      {
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
  bandWidthNode = parseBandWidth(s);
  if (bandWidthNode == NULL)
  {
    stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth value '%s'",value);
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
    userId  = File_userNameToUserId(userName);
    groupId = File_groupNameToGroupId(groupName);
  }
  else if (String_scanCString(value,"%256s:",userName))
  {
    userId  = File_userNameToUserId(userName);
    groupId = FILE_DEFAULT_GROUP_ID;
  }
  else if (String_scanCString(value,":%256s",groupName))
  {
    userId  = FILE_DEFAULT_USER_ID;
    groupId = File_groupNameToGroupId(groupName);
  }
  else if (String_scanCString(value,"%256s",userName))
  {
    userId  = File_userNameToUserId(userName);
    groupId = FILE_DEFAULT_GROUP_ID;
  }
  else
  {
    stringFormat(errorMessage,errorMessageSize,"Unknown owner ship value '%s'",value);
    return FALSE;
  }

  // store owner values
  ((JobOptionsOwner*)variable)->userId  = userId;
  ((JobOptionsOwner*)variable)->groupId = groupId;

  return TRUE;
}

#ifdef MULTI_CRYPT
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
  StringTokenizer stringTokenizer;
  ConstString     token;
  CryptAlgorithms cryptAlgorithms[4];
  uint            cryptAlgorithmCount;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

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
    if (!Crypt_parseAlgorithm(String_cString(token),&((CryptAlgorithms*)variable)[cryptAlgorithmCount]))
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown crypt algorithm '%s'",String_cString(token));
      String_doneTokenizer(&stringTokenizer);
      return FALSE;
    }
    if (((CryptAlgorithms*)variable)[cryptAlgorithmCount] != CRYPT_ALGORITHM_NONE)
    {
      cryptAlgorithmCount++;
    }
  }
  String_doneTokenizer(&stringTokenizer);
  while (cryptAlgorithmCount < 4)
  {
    ((CryptAlgorithms*)variable)[cryptAlgorithmCount] = CRYPT_ALGORITHM_NONE;
    cryptAlgorithmCount++;
  }

  return TRUE;
}
#endif /* MULTI_CRYPT */


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

  if ((*(Password**)variable) == NULL)
  {
    (*(Password**)variable) = Password_new();
  }
  Password_setCString(*(Password**)variable,value);

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
    stringSet(errorMessage,Error_getText(error),errorMessageSize);
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
  UNUSED_VARIABLE(defaultValue);

  if (File_existsCString(value))
  {
    // read key data from file

    error = readKeyFile(key,value);
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,Error_getText(error),errorMessageSize);
      return error;
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
      data = Password_allocSecure(dataLength);
      if (data == NULL)
      {
        stringSet(errorMessage,"insufficient secure memory",errorMessageSize);
        return FALSE;
      }

      // decode base64
      if (!Misc_base64DecodeCString((byte*)data,dataLength,&value[7]))
      {
        stringSet(errorMessage,"decode base64 fail",errorMessageSize);
        Password_freeSecure(data);
        return FALSE;
      }

      // set key data
      if (key->data != NULL) Password_freeSecure(key->data);
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
      data = Password_allocSecure(dataLength);
      if (data == NULL)
      {
        stringSet(errorMessage,"insufficient secure memory",errorMessageSize);
        return FALSE;
      }

      // copy data
      memcpy(data,value,dataLength);

      // set key data
      if (key->data != NULL) Password_freeSecure(key->data);
      key->data   = data;
      key->length = dataLength;
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseCryptKey
* Purpose: command line option call back for get crypt key (without
*          password and salt)
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseCryptKey(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  CryptKey *cryptKey = (CryptKey*)variable;
  Errors   error;
  String   fileName;
  uint     dataLength;
  void     *data;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);

  // get key data
  if (File_existsCString(value))
  {
    // read key file (base64 encoded)

    fileName = String_newCString(value);
    error = Crypt_readPublicPrivateKeyFile(cryptKey,
                                           fileName,
                                           CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                           CRYPT_KEY_DERIVE_NONE,
                                           NULL,  // password
                                           NULL,  // salt
                                           0  // saltLength
                                          );
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,Error_getText(error),errorMessageSize);
      String_delete(fileName);
      return FALSE;
    }
    String_delete(fileName);
  }
  else if (stringStartsWith(value,"base64:"))
  {
    // base64 encoded key

    // get key data length
    dataLength = Misc_base64DecodeLengthCString(&value[7]);
    if (dataLength > 0)
    {
      // allocate key memory
      data = Password_allocSecure(dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // decode base64
      if (!Misc_base64DecodeCString((byte*)data,dataLength,&value[7]))
      {
        Password_freeSecure(data);
        return FALSE;
      }

//TODO: use Crypt_setKeyString
      // set crypt key data
      error = Crypt_setPublicPrivateKeyData(cryptKey,
                                            data,
                                            dataLength,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // password
                                            NULL,  // salt
                                            0  // saltLength
                                           );
      if (error != ERROR_NONE)
      {
        stringSet(errorMessage,Error_getText(error),errorMessageSize);
        return FALSE;
      }

      // free resources
      Password_freeSecure(data);
    }
  }
  else
  {
    // plain key string

    // set crypt key data
    error = Crypt_setPublicPrivateKeyData(cryptKey,
                                          value,
                                          strlen(value),
                                          CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // password
                                          NULL,  // salt
                                          0  // saltLength
                                         );
    if (error != ERROR_NONE)
    {
      stringSet(errorMessage,Error_getText(error),errorMessageSize);
      return FALSE;
    }
  }

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
                                    NULL,  // deviceName
                                    TRUE  // alwaysUnmount
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

//TODO: remove
LOCAL bool cmdOptionParseDeprecatedStopOnError(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(bool*)variable) = !(   (stringEqualsIgnoreCase(value,"1") == 0)
                         || (stringEqualsIgnoreCase(value,"true") == 0)
                         || (stringEqualsIgnoreCase(value,"on") == 0)
                         || (stringEqualsIgnoreCase(value,"yes") == 0)
                        );

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseConfigFile
* Purpose: command line option call back for parsing configuration file
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  StringList_appendCString(&configFileNameList,value);

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
  printf("       %s [<options>] [--] <key file name>\n",programName);
  printf("\n");
  printf("Archive name:  <file name>\n");
  printf("               file://<file name>\n");
  printf("               ftp:// [<login name>[:<password>]@]<host name>/<file name>\n");
  printf("               scp:// [<login name>[:<password>]@]<host name>[:<port>]/<file name>\n");
  printf("               sftp:// [<login name>[:<password>]@]<host name>[:<port>]/<file name>\n");
  printf("               webdav:// [<login name>[:<password>]@]<host name>/<file name>\n");
  printf("               cd:// [<device name>:]<file name>\n");
  printf("               dvd:// [<device name>:]<file name>\n");
  printf("               bd:// [<device name>:]<file name>\n");
  printf("               device:// [<device name>:]<file name>\n");
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
  memset(&globalOptions,0,sizeof(GlobalOptions));

  globalOptions.runMode                                         = RUN_MODE_INTERACTIVE;;
  globalOptions.barExecutable                                   = NULL;
  globalOptions.niceLevel                                       = 0;
  globalOptions.maxThreads                                      = 0;
  globalOptions.tmpDirectory                                    = String_newCString(DEFAULT_TMP_DIRECTORY);
  globalOptions.maxTmpSize                                      = 0LL;
  List_init(&globalOptions.maxBandWidthList);
  globalOptions.maxBandWidthList.n                              = 0L;
  globalOptions.maxBandWidthList.lastReadTimestamp              = 0LL;
  globalOptions.fragmentSize                                    = DEFAULT_FRAGMENT_SIZE;
  globalOptions.compressMinFileSize                             = DEFAULT_COMPRESS_MIN_FILE_SIZE;
  globalOptions.continuousMaxSize                               = 0LL;
  globalOptions.cryptPassword                                   = NULL;
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

  List_init(&globalOptions.serverList);
  Semaphore_init(&globalOptions.serverList.lock);
  List_init(&globalOptions.deviceList);
  Semaphore_init(&globalOptions.deviceList.lock);

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

  globalOptions.indexDatabaseAutoUpdateFlag                     = TRUE;
  List_init(&globalOptions.indexDatabaseMaxBandWidthList);
  globalOptions.indexDatabaseMaxBandWidthList.n                 = 0L;
  globalOptions.indexDatabaseMaxBandWidthList.lastReadTimestamp = 0LL;
  globalOptions.indexDatabaseKeepTime                           = 24*60*60;  // 1 day

  globalOptions.groupFlag                                       = FALSE;
  globalOptions.allFlag                                         = FALSE;
  globalOptions.longFormatFlag                                  = FALSE;
  globalOptions.noHeaderFooterFlag                              = FALSE;
  globalOptions.deleteOldArchiveFilesFlag                       = FALSE;
  globalOptions.noDefaultConfigFlag                             = FALSE;
  globalOptions.quietFlag                                       = FALSE;
  globalOptions.verboseLevel                                    = DEFAULT_VERBOSE_LEVEL;

  globalOptions.serverDebugFlag                                 = FALSE;
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
  List_done(&globalOptions.indexDatabaseMaxBandWidthList,CALLBACK((ListNodeFreeFunction)freeBandWidthNode,NULL));
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

  Semaphore_done(&globalOptions.deviceList.lock);
  List_done(&globalOptions.deviceList,CALLBACK((ListNodeFreeFunction)freeDeviceNode,NULL));
  Semaphore_done(&globalOptions.serverList.lock);
  List_done(&globalOptions.serverList,CALLBACK((ListNodeFreeFunction)freeServerNode,NULL));

  Crypt_doneKey(&globalOptions.signaturePrivateKey);
  Crypt_doneKey(&globalOptions.signaturePublicKey);
  if (globalOptions.cryptPassword != NULL) Password_done(globalOptions.cryptPassword);
  List_done(&globalOptions.maxBandWidthList,CALLBACK((ListNodeFreeFunction)freeBandWidthNode,NULL));
  String_delete(globalOptions.tmpDirectory);
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
  struct sigaction signalAction;
  AutoFreeList     autoFreeList;
  Errors           error;
  const char       *localePath;
  String           fileName;

  // initialize crash dump handler
  #if HAVE_BREAKPAD
    if (!MiniDump_init())
    {
      (void)fprintf(stderr,"Warning: Cannot initialize crash dump handler. No crash dumps will be created.\n");
    }
  #endif /* HAVE_BREAKPAD */

  // install signal handlers
  sigfillset(&signalAction.sa_mask);
  signalAction.sa_flags     = SA_SIGINFO;
  signalAction.sa_sigaction = signalHandler;
  sigaction(SIGSEGV,&signalAction,NULL);
  sigaction(SIGFPE,&signalAction,NULL);
  sigaction(SIGILL,&signalAction,NULL);
  sigaction(SIGBUS,&signalAction,NULL);
  sigaction(SIGTERM,&signalAction,NULL);
  sigaction(SIGINT,&signalAction,NULL);

  // initialize variables
  AutoFree_init(&autoFreeList);
  tmpDirectory = String_new();
  pidFileName  = NULL;
  AUTOFREE_ADD(&autoFreeList,tmpDirectory,{ String_delete(tmpDirectory); });

  Semaphore_init(&consoleLock);
  DEBUG_TESTCODE() { Semaphore_done(&consoleLock); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&consoleLock,{ Semaphore_done(&consoleLock); });

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

  error = Continuous_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
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

  error = Server_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Server_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Server_initAll,{ Server_doneAll(); });

  // initialize variables
  initGlobalOptions();

  command                                = COMMAND_NONE;
  jobName                                = NULL;
  uuid                                   = NULL;
  storageName                            = NULL;
  initJobOptions(&jobOptions);

  EntryList_init(&includeEntryList);
  includeFileCommand                     = NULL;
  includeImageCommand                    = NULL;
  PatternList_init(&excludePatternList);
  excludeCommand                         = NULL;
  PatternList_init(&compressExcludePatternList);
  List_init(&mountList);
  DeltaSourceList_init(&deltaSourceList);
  initServer(&defaultFileServer,NULL,SERVER_TYPE_FILE);
  initServer(&defaultFTPServer,NULL,SERVER_TYPE_FTP);
  initServer(&defaultSSHServer,NULL,SERVER_TYPE_SSH);
  initServer(&defaultWebDAVServer,NULL,SERVER_TYPE_WEBDAV);
  initDevice(&defaultDevice);
  serverPort                             = DEFAULT_SERVER_PORT;
  serverTLSPort                          = DEFAULT_TLS_SERVER_PORT;
  serverCA.data                          = NULL;
  serverCA.length                        = 0;
  serverCert.data                        = NULL;
  serverCert.length                      = 0;
  serverKey.data                         = NULL;
  serverKey.length                       = 0;
  serverPassword                         = Password_new();
  serverMaxConnections                   = DEFAULT_MAX_SERVER_CONNECTIONS;
  serverJobsDirectory                    = DEFAULT_JOBS_DIRECTORY;

  continuousDatabaseFileName             = NULL;
  indexDatabaseFileName                  = NULL;

  logTypes                               = LOG_TYPE_NONE;
  logFileName                            = NULL;
  logFormat                              = DEFAULT_LOG_FORMAT;
  logPostCommand                         = NULL;

  batchFlag                              = FALSE;

  keyFileName                            = NULL;
  keyBits                                = MIN_ASYMMETRIC_CRYPT_KEY_BITS;

  StringList_init(&configFileNameList);

  Semaphore_init(&logLock);
  logFile                                = NULL;
  POSIXLocale                            = newlocale(LC_ALL,"POSIX",0);

  Thread_initLocalVariable(&outputLineHandle,outputLineInit,NULL);
  lastOutputLine                         = NULL;

  // initialize default ssh keys
  fileName = String_new();
  File_appendFileNameCString(String_setCString(fileName,getenv("HOME")),".ssh/id_rsa.pub");
  if (File_exists(fileName))
  {
    (void)readKeyFile(&defaultSSHServer.ssh.publicKey,String_cString(fileName));
    (void)readKeyFile(&defaultWebDAVServer.webDAV.publicKey,String_cString(fileName));
  }
  File_appendFileNameCString(String_setCString(fileName,getenv("HOME")),".ssh/id_rsa");
  if (File_exists(fileName))
  {
    (void)readKeyFile(&defaultSSHServer.ssh.privateKey,String_cString(fileName));
    (void)readKeyFile(&defaultWebDAVServer.webDAV.privateKey,String_cString(fileName));
  }
  String_delete(fileName);

  // read default server CA, certificate, key
  (void)readCertificateFile(&serverCA,DEFAULT_TLS_SERVER_CA_FILE);
  (void)readCertificateFile(&serverCert,DEFAULT_TLS_SERVER_CERTIFICATE_FILE);
  (void)readKeyFile(&serverKey,DEFAULT_TLS_SERVER_KEY_FILE);

  // initialize command line options and config values
  ConfigValue_init(CONFIG_VALUES);
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));

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
  struct sigaction signalAction;

  // deinitialize command line options and config values
  CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  ConfigValue_done(CONFIG_VALUES);

  // done server ca, cert, key
  doneKey(&serverKey);
  doneCertificate(&serverCert);
  doneCertificate(&serverCA);

  // deinitialize variables
  freelocale(POSIXLocale);
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
  doneServer(&defaultWebDAVServer);
  doneServer(&defaultSSHServer);
  doneServer(&defaultFTPServer);
  doneServer(&defaultFileServer);
  DeltaSourceList_done(&deltaSourceList);
  List_done(&mountList,CALLBACK((ListNodeFreeFunction)freeMountNode,NULL));
  PatternList_done(&compressExcludePatternList);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  Password_delete(serverPassword);

  doneJobOptions(&jobOptions);
  String_delete(storageName);
  String_delete(uuid);
  String_delete(jobName);
  doneGlobalOptions();
  Thread_doneLocalVariable(&outputLineHandle,outputLineDone,NULL);
  String_delete(tmpDirectory);
  StringList_done(&configFileNameList);
  String_delete(keyFileName);

  // deinitialize modules
  Server_doneAll();
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

  Semaphore_done(&consoleLock);

  // deinstall signal handlers
  sigfillset(&signalAction.sa_mask);
  signalAction.sa_flags   = 0;
  signalAction.sa_handler = SIG_DFL;
  sigaction(SIGINT,&signalAction,NULL);
  sigaction(SIGQUIT,&signalAction,NULL);
  sigaction(SIGTERM,&signalAction,NULL);
  sigaction(SIGBUS,&signalAction,NULL);
  sigaction(SIGILL,&signalAction,NULL);
  sigaction(SIGFPE,&signalAction,NULL);
  sigaction(SIGSEGV,&signalAction,NULL);

  // deinitialize crash dump handler
  #if HAVE_BREAKPAD
    MiniDump_done();
  #endif /* HAVE_BREAKPAD */
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
    if (!File_exists(globalOptions.tmpDirectory)) { printError("Temporary directory '%s' does not exists!\n",String_cString(globalOptions.tmpDirectory)); return FALSE; }
    if (!File_isDirectory(globalOptions.tmpDirectory)) { printError("'%s' is not a directory!\n",String_cString(globalOptions.tmpDirectory)); return FALSE; }
    if (!File_isWriteable(globalOptions.tmpDirectory)) { printError("Temporary directory '%s' is not writeable!\n",String_cString(globalOptions.tmpDirectory)); return FALSE; }
  }

  return TRUE;
}

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
  SemaphoreLock semaphoreLock;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (logFileName != NULL)
    {
      logFile = fopen(logFileName,"a");
      if (logFile == NULL) printWarning("Cannot open log file '%s' (error: %s)!\n",logFileName,strerror(errno));
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
  SemaphoreLock semaphoreLock;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
  SemaphoreLock semaphoreLock;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (logFileName != NULL)
    {
      fclose(logFile);
      logFile = fopen(logFileName,"a");
      if (logFile == NULL) printWarning("Cannot re-open log file '%s' (error: %s)!\n",logFileName,strerror(errno));
    }
  }
}

/*---------------------------------------------------------------------*/

String getConfigFileName(String fileName)
{
  StringNode *stringNode;

  assert(fileName != NULL);

  String_clear(fileName);

  stringNode = STRINGLIST_FIND_LAST(&configFileNameList,configFileName,File_isWriteable(configFileName));
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
  SemaphoreLock     semaphoreLock;
  ServerNode        *serverNode;

  // init variables
  configFileName = String_new();
  StringList_init(&configLinesList);
  line           = String_new();

  // get config file name
  getConfigFileName(configFileName);
  if (String_isEmpty(configFileName))
  {
    String_delete(configFileName);
    return ERROR_NO_FILE_NAME;
  }

  // read file
  error = ConfigValue_readConfigFileLines(configFileName,&configLinesList);
  if (error != ERROR_NONE)
  {
    StringList_done(&configLinesList);
    String_delete(line);
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->server.type == SERVER_TYPE_FILE)
      {
        // insert new schedule sections
        String_format(String_clear(line),"[file-server %'S]",serverNode->server.name);
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->server.type == SERVER_TYPE_FTP)
      {
        // insert new schedule sections
        String_format(String_clear(line),"[ftp-server %'S]",serverNode->server.name);
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->server.type == SERVER_TYPE_SSH)
      {
        // insert new schedule sections
        String_format(String_clear(line),"[ssh-server %'S]",serverNode->server.name);
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      if (serverNode->server.type == SERVER_TYPE_WEBDAV)
      {
        // insert new schedule sections
        String_format(String_clear(line),"[webdav-server %'S]",serverNode->server.name);
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

  // write file
  error = ConfigValue_writeConfigFileLines(configFileName,&configLinesList);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    StringList_done(&configLinesList);
    String_delete(configFileName);
    return error;
  }

  // free resources
  String_delete(line);
  StringList_done(&configLinesList);
  String_delete(configFileName);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

const char *getArchiveTypeName(ArchiveTypes archiveType)
{
  const char *text;

  text = NULL;
  switch (archiveType)
  {
    case ARCHIVE_TYPE_NORMAL:       text = "normal";       break;
    case ARCHIVE_TYPE_FULL:         text = "full";         break;
    case ARCHIVE_TYPE_INCREMENTAL:  text = "incremental";  break;
    case ARCHIVE_TYPE_DIFFERENTIAL: text = "differential"; break;
    case ARCHIVE_TYPE_CONTINUOUS:   text = "continuous";   break;
    case ARCHIVE_TYPE_UNKNOWN:      text = "unknown";      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return text;
}

const char *getArchiveTypeShortName(ArchiveTypes archiveType)
{
  const char *text;

  text = NULL;
  switch (archiveType)
  {
    case ARCHIVE_TYPE_NORMAL:       text = "N"; break;
    case ARCHIVE_TYPE_FULL:         text = "F"; break;
    case ARCHIVE_TYPE_INCREMENTAL:  text = "I"; break;
    case ARCHIVE_TYPE_DIFFERENTIAL: text = "D"; break;
    case ARCHIVE_TYPE_CONTINUOUS:   text = "C"; break;
    case ARCHIVE_TYPE_UNKNOWN:      text = "U"; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return text;
}

const char *getPasswordTypeName(PasswordTypes passwordType)
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
  String        line;
  SemaphoreLock semaphoreLock;

  assert(format != NULL);

  if (isPrintInfo(verboseLevel))
  {
    line = String_new();

    // format line
    if (prefix != NULL) String_appendCString(line,prefix);
    String_vformat(line,format,arguments);

    // output
    SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
  ulong z;

  assert(file != NULL);
  assert(saveLine != NULL);
  assert(Semaphore_isLocked(&consoleLock));

  (*saveLine) = String_new();

  if (File_isTerminal(file))
  {
    // wipe-out last line
    if (lastOutputLine != NULL)
    {
      for (z = 0; z < String_length(lastOutputLine); z++)
      {
        (void)fwrite("\b",1,1,file);
      }
      for (z = 0; z < String_length(lastOutputLine); z++)
      {
        (void)fwrite(" ",1,1,file);
      }
      for (z = 0; z < String_length(lastOutputLine); z++)
      {
        (void)fwrite("\b",1,1,file);
      }
      fflush(file);
    }

    // save last line
    String_set(*saveLine,lastOutputLine);
  }
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

void printConsole(FILE *file, const char *format, ...)
{
  String        line;
  va_list       arguments;
  SemaphoreLock semaphoreLock;

  assert(file != NULL);
  assert(format != NULL);

  line = String_new();

  // format line
  va_start(arguments,format);
  String_vformat(line,format,arguments);
  va_end(arguments);

  // output
  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    outputConsole(file,line);
  }

  String_delete(line);
}

void printWarning(const char *text, ...)
{
  va_list       arguments;
  String        line;
  String        saveLine;
  SemaphoreLock semaphoreLock;

  assert(text != NULL);

  // output log line
  va_start(arguments,text);
  vlogMessage(NULL,LOG_TYPE_WARNING,"Warning",text,arguments);
  va_end(arguments);

  // output line
  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"Warning: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    saveConsole(stderr,&saveLine);
    (void)fwrite(String_cString(line),1,String_length(line),stderr);
    restoreConsole(stderr,&saveLine);
  }
  String_delete(line);
}

void printError(const char *text, ...)
{
  va_list       arguments;
  String        saveLine;
  String        line;
  SemaphoreLock semaphoreLock;

  assert(text != NULL);

  // output log line
  va_start(arguments,text);
  vlogMessage(NULL,LOG_TYPE_ERROR,"ERROR",text,arguments);
  va_end(arguments);

  // output line
  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"ERROR: ");
  String_vformat(line,text,arguments);
  va_end(arguments);
  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    saveConsole(stderr,&saveLine);
    (void)fwrite(String_cString(line),1,String_length(line),stderr);
    restoreConsole(stderr,&saveLine);
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
    File_delete(logHandle->logFileName,FALSE);
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
    File_delete(logHandle->logFileName,FALSE);
    String_delete(logHandle->logFileName); logHandle->logFileName = NULL;
  }
}

void vlogMessage(LogHandle *logHandle, ulong logType, const char *prefix, const char *text, va_list arguments)
{
  static uint64 lastReopenTimestamp = 0LL;

  SemaphoreLock semaphoreLock;
  String        dateTime;
  va_list       tmpArguments;
  uint64        nowTimestamp;

  assert(text != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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

void templateInit(TemplateHandle   *templateHandle,
                  const char       *templateString,
                  ExpandMacroModes expandMacroMode,
                  time_t           time,
                  bool             initFinalFlag
                 )
{
  assert(templateHandle != NULL);

  // init variables
  templateHandle->templateString  = templateString;
  templateHandle->expandMacroMode = expandMacroMode;
  templateHandle->time            = time;
  templateHandle->initFinalFlag   = initFinalFlag;
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

  // get week numbers
  #ifdef HAVE_LOCALTIME_R
    tm = localtime_r(&templateHandle->time,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    tm = localtime(&templateHandle->time);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);
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
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
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
                      time_t           time,
                      bool             initFinalFlag,
                      const TextMacro  textMacros[],
                      uint             textMacroCount
                     )
{
  TemplateHandle templateHandle;

  templateInit(&templateHandle,
               templateString,
               expandMacroMode,
               time,
               initFinalFlag
              );
  templateMacros(&templateHandle,
                 textMacros,
                 textMacroCount
                );

  return templateDone(&templateHandle,
                      NULL  // string
                     );
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
                    ConstString      jobName,
                    const JobOptions *jobOptions,
                    ArchiveTypes     archiveType,
                    ConstString      scheduleCustomText
                   )
{
  String     command;
  TextMacro  textMacros[5];
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
        TEXT_MACRO_N_STRING (textMacros[0],"%file",logHandle->logFileName,              TEXT_MACRO_PATTERN_STRING);
        TEXT_MACRO_N_STRING (textMacros[1],"%name",jobName,                             TEXT_MACRO_PATTERN_STRING);
        TEXT_MACRO_N_CSTRING(textMacros[2],"%type",getArchiveTypeName(archiveType),     TEXT_MACRO_PATTERN_STRING);
        TEXT_MACRO_N_CSTRING(textMacros[3],"%T",   getArchiveTypeShortName(archiveType),".");
        TEXT_MACRO_N_STRING (textMacros[4],"%text",scheduleCustomText,                  TEXT_MACRO_PATTERN_STRING);
        Misc_expandMacros(command,
                          logPostCommand,
                          EXPAND_MACRO_MODE_STRING,
                          textMacros,SIZE_OF_ARRAY(textMacros),
                          TRUE
                         );
        printInfo(2,"Log post process '%s'...\n",String_cString(command));
        assert(logHandle->logFileName != NULL);

        StringList_init(&stderrList);
        error = Misc_executeCommand(logPostCommand,
                                    textMacros,SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(NULL,NULL),
                                    CALLBACK(executeIOlogPostProcess,&stderrList)
                                   );
        if (error != ERROR_NONE)
        {
          printError("Cannot post-process log file (error: %s)\n",Error_getText(error));
          STRINGLIST_ITERATE(&stderrList,stringNode,string)
          {
            printError("  %s\n",String_cString(string));
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
          printWarning("Cannot re-open log file '%s' (error: %s)\n",String_cString(logHandle->logFileName),strerror(errno));
        }
      }

      // free resources
      String_delete(command);
    }
  }
}

void initJobOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  memset(jobOptions,0,sizeof(JobOptions));
  jobOptions->archiveType                     = ARCHIVE_TYPE_NORMAL;
  jobOptions->archivePartSize                 = 0LL;
  jobOptions->incrementalListFileName         = String_new();
  jobOptions->directoryStripCount             = DIRECTORY_STRIP_NONE;
  jobOptions->destination                     = String_new();
  jobOptions->owner.userId                    = FILE_DEFAULT_USER_ID;
  jobOptions->owner.groupId                   = FILE_DEFAULT_GROUP_ID;
  jobOptions->patternType                     = PATTERN_TYPE_GLOB;
  jobOptions->compressAlgorithms.delta        = COMPRESS_ALGORITHM_NONE;
  jobOptions->compressAlgorithms.byte         = COMPRESS_ALGORITHM_NONE;
  jobOptions->cryptAlgorithms[0]              = CRYPT_ALGORITHM_NONE;
  jobOptions->cryptAlgorithms[1]              = CRYPT_ALGORITHM_NONE;
  jobOptions->cryptAlgorithms[2]              = CRYPT_ALGORITHM_NONE;
  jobOptions->cryptAlgorithms[3]              = CRYPT_ALGORITHM_NONE;
  #ifdef HAVE_GCRYPT
    jobOptions->cryptType                     = CRYPT_TYPE_SYMMETRIC;
  #else /* not HAVE_GCRYPT */
    jobOptions->cryptType                     = CRYPT_TYPE_NONE;
  #endif /* HAVE_GCRYPT */
  jobOptions->cryptPasswordMode               = PASSWORD_MODE_DEFAULT;
  initKey(&jobOptions->cryptPublicKey);
  initKey(&jobOptions->cryptPrivateKey);
  jobOptions->preProcessScript                = NULL;
  jobOptions->postProcessScript               = NULL;
  jobOptions->maxStorageSize                  = 0LL;
  jobOptions->volumeSize                      = 0LL;
  jobOptions->comment                         = String_new();
  jobOptions->skipUnreadableFlag              = TRUE;
  jobOptions->forceDeltaCompressionFlag       = FALSE;
  jobOptions->ignoreNoDumpAttributeFlag       = FALSE;
  jobOptions->archiveFileMode                 = ARCHIVE_FILE_MODE_STOP;
  jobOptions->overwriteEntriesFlag            = FALSE;
  jobOptions->errorCorrectionCodesFlag        = FALSE;
  jobOptions->blankFlag                       = FALSE;
  jobOptions->alwaysCreateImageFlag           = FALSE;
  jobOptions->waitFirstVolumeFlag             = FALSE;
  jobOptions->rawImagesFlag                   = FALSE;
  jobOptions->noFragmentsCheckFlag            = FALSE;
  jobOptions->noIndexDatabaseFlag             = FALSE;
  jobOptions->dryRunFlag                      = FALSE;
  jobOptions->skipVerifySignaturesFlag        = FALSE;
  jobOptions->noStorageFlag                   = FALSE;
  jobOptions->noBAROnMediumFlag               = FALSE;
  jobOptions->noStopOnErrorFlag               = FALSE;

  DEBUG_ADD_RESOURCE_TRACE(jobOptions,sizeof(JobOptions));
}

void initDuplicateJobOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions)
{
  assert(jobOptions != NULL);
  assert(fromJobOptions != NULL);

  memcpy(jobOptions,fromJobOptions,sizeof(JobOptions));
  jobOptions->incrementalListFileName             = String_duplicate(fromJobOptions->incrementalListFileName);
  jobOptions->destination                         = String_duplicate(fromJobOptions->destination);

  jobOptions->cryptPassword                       = Password_duplicate(fromJobOptions->cryptPassword);
  duplicateKey(&jobOptions->cryptPublicKey,&fromJobOptions->cryptPublicKey);
  duplicateKey(&jobOptions->cryptPrivateKey,&fromJobOptions->cryptPrivateKey);

  jobOptions->preProcessScript                    = String_duplicate(fromJobOptions->preProcessScript);
  jobOptions->postProcessScript                   = String_duplicate(fromJobOptions->postProcessScript);

  jobOptions->comment                             = String_duplicate(fromJobOptions->comment);

  jobOptions->ftpServer.loginName                 = String_duplicate(fromJobOptions->ftpServer.loginName);
  jobOptions->ftpServer.password                  = Password_duplicate(fromJobOptions->ftpServer.password);

  jobOptions->sshServer.loginName                 = String_duplicate(fromJobOptions->sshServer.loginName);
  jobOptions->sshServer.password                  = Password_duplicate(fromJobOptions->sshServer.password);
  duplicateKey(&jobOptions->sshServer.publicKey,&fromJobOptions->sshServer.publicKey);
  duplicateKey(&jobOptions->sshServer.privateKey,&fromJobOptions->sshServer.privateKey);

  jobOptions->webDAVServer.loginName              = String_duplicate(fromJobOptions->webDAVServer.loginName);
  jobOptions->webDAVServer.password               = Password_duplicate(fromJobOptions->webDAVServer.password);
  duplicateKey(&jobOptions->webDAVServer.publicKey,&fromJobOptions->webDAVServer.publicKey);
  duplicateKey(&jobOptions->webDAVServer.privateKey,&fromJobOptions->webDAVServer.privateKey);

  jobOptions->opticalDisk.requestVolumeCommand    = String_duplicate(fromJobOptions->opticalDisk.requestVolumeCommand);
  jobOptions->opticalDisk.unloadVolumeCommand     = String_duplicate(fromJobOptions->opticalDisk.unloadVolumeCommand);
  jobOptions->opticalDisk.imagePreProcessCommand  = String_duplicate(fromJobOptions->opticalDisk.imagePreProcessCommand);
  jobOptions->opticalDisk.imagePostProcessCommand = String_duplicate(fromJobOptions->opticalDisk.imagePostProcessCommand);
  jobOptions->opticalDisk.imageCommand            = String_duplicate(fromJobOptions->opticalDisk.imageCommand);
  jobOptions->opticalDisk.eccPreProcessCommand    = String_duplicate(fromJobOptions->opticalDisk.eccPreProcessCommand);
  jobOptions->opticalDisk.eccPostProcessCommand   = String_duplicate(fromJobOptions->opticalDisk.eccPostProcessCommand);
  jobOptions->opticalDisk.eccCommand              = String_duplicate(fromJobOptions->opticalDisk.eccCommand);
  jobOptions->opticalDisk.blankCommand            = String_duplicate(fromJobOptions->opticalDisk.blankCommand);
  jobOptions->opticalDisk.writePreProcessCommand  = String_duplicate(fromJobOptions->opticalDisk.writePreProcessCommand);
  jobOptions->opticalDisk.writePostProcessCommand = String_duplicate(fromJobOptions->opticalDisk.writePostProcessCommand);
  jobOptions->opticalDisk.writeCommand            = String_duplicate(fromJobOptions->opticalDisk.writeCommand);

  jobOptions->deviceName                          = String_duplicate(fromJobOptions->deviceName);
  jobOptions->device.requestVolumeCommand         = String_duplicate(fromJobOptions->device.requestVolumeCommand);
  jobOptions->device.unloadVolumeCommand          = String_duplicate(fromJobOptions->device.unloadVolumeCommand);
  jobOptions->device.imagePreProcessCommand       = String_duplicate(fromJobOptions->device.imagePreProcessCommand);
  jobOptions->device.imagePostProcessCommand      = String_duplicate(fromJobOptions->device.imagePostProcessCommand);
  jobOptions->device.imageCommand                 = String_duplicate(fromJobOptions->device.imageCommand);
  jobOptions->device.eccPreProcessCommand         = String_duplicate(fromJobOptions->device.eccPreProcessCommand);
  jobOptions->device.eccPostProcessCommand        = String_duplicate(fromJobOptions->device.eccPostProcessCommand);
  jobOptions->device.eccCommand                   = String_duplicate(fromJobOptions->device.eccCommand);
  jobOptions->device.blankCommand                 = String_duplicate(fromJobOptions->device.blankCommand);
  jobOptions->device.writePreProcessCommand       = String_duplicate(fromJobOptions->device.writePreProcessCommand);
  jobOptions->device.writePostProcessCommand      = String_duplicate(fromJobOptions->device.writePostProcessCommand);
  jobOptions->device.writeCommand                 = String_duplicate(fromJobOptions->device.writeCommand);

  DEBUG_ADD_RESOURCE_TRACE(jobOptions,sizeof(JobOptions));
}

void doneJobOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(jobOptions,sizeof(JobOptions));

  String_delete(jobOptions->device.writeCommand);
  String_delete(jobOptions->device.writePostProcessCommand);
  String_delete(jobOptions->device.writePreProcessCommand);
  String_delete(jobOptions->device.blankCommand);
  String_delete(jobOptions->device.eccCommand);
  String_delete(jobOptions->device.eccPostProcessCommand);
  String_delete(jobOptions->device.eccPreProcessCommand);
  String_delete(jobOptions->device.imageCommand);
  String_delete(jobOptions->device.imagePostProcessCommand);
  String_delete(jobOptions->device.imagePreProcessCommand);
  String_delete(jobOptions->device.unloadVolumeCommand);
  String_delete(jobOptions->device.requestVolumeCommand);
  String_delete(jobOptions->deviceName);

  String_delete(jobOptions->opticalDisk.writeCommand);
  String_delete(jobOptions->opticalDisk.writePostProcessCommand);
  String_delete(jobOptions->opticalDisk.writePreProcessCommand);
  String_delete(jobOptions->opticalDisk.blankCommand);
  String_delete(jobOptions->opticalDisk.eccCommand);
  String_delete(jobOptions->opticalDisk.eccPostProcessCommand);
  String_delete(jobOptions->opticalDisk.eccPreProcessCommand);
  String_delete(jobOptions->opticalDisk.imageCommand);
  String_delete(jobOptions->opticalDisk.imagePostProcessCommand);
  String_delete(jobOptions->opticalDisk.imagePreProcessCommand);
  String_delete(jobOptions->opticalDisk.unloadVolumeCommand);
  String_delete(jobOptions->opticalDisk.requestVolumeCommand);

  doneKey(&jobOptions->webDAVServer.privateKey);
  doneKey(&jobOptions->webDAVServer.publicKey);
  Password_delete(jobOptions->webDAVServer.password);
  String_delete(jobOptions->webDAVServer.loginName);

  doneKey(&jobOptions->sshServer.privateKey);
  doneKey(&jobOptions->sshServer.publicKey);
  Password_delete(jobOptions->sshServer.password);
  String_delete(jobOptions->sshServer.loginName);

  Password_delete(jobOptions->ftpServer.password);
  String_delete(jobOptions->ftpServer.loginName);

  String_delete(jobOptions->comment);

  String_delete(jobOptions->postProcessScript);
  String_delete(jobOptions->preProcessScript);

  doneKey(&jobOptions->cryptPrivateKey);
  doneKey(&jobOptions->cryptPublicKey);
  Password_delete(jobOptions->cryptPassword);

  String_delete(jobOptions->destination);
  String_delete(jobOptions->incrementalListFileName);
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

bool duplicateKey(Key *toKey, const Key *fromKey)
{
  uint length;
  void *data;

  assert(toKey != NULL);

  if (fromKey != NULL)
  {
    length = fromKey->length;
    data = Password_allocSecure(length);
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

  toKey->data   = data;
  toKey->length = length;

  return TRUE;
}

void doneKey(Key *key)
{
  assert(key != NULL);

  if (key->data != NULL)
  {
    Password_freeSecure(key->data);
  }
}

bool isKeyAvailable(const Key *key)
{
  assert(key != NULL);

  return key->data != NULL;
}

void clearKey(Key *key)
{
  assert(key != NULL);

  if (key->data != NULL) Password_freeSecure(key->data);
  key->data   = NULL;
  key->length = 0;
}

bool setKey(Key *key, const void *keyData, uint keyLength)
{
  void *data;

  assert(key != NULL);

  data = Password_allocSecure(keyLength);
  if (data == NULL)
  {
    return FALSE;
  }
  memcpy(data,keyData,keyLength);

  if (key->data != NULL) Password_freeSecure(key->data);
  key->data   = data;
  key->length = keyLength;

  return TRUE;
}

bool setKeyString(Key *key, ConstString string)
{
  return setKey(key,String_cString(string),String_length(string));
}

void initServer(Server *server, ConstString name, ServerTypes serverType)
{
  assert(server != NULL);

  server->name                    = (name != NULL) ? String_duplicate(name) : String_new();
  server->type                    = serverType;
  switch (serverType)
  {
    case SERVER_TYPE_NONE:
      break;
    case SERVER_TYPE_FILE:
      break;
    case SERVER_TYPE_FTP:
      server->ftp.loginName             = NULL;
      server->ftp.password              = NULL;
      break;
    case SERVER_TYPE_SSH:
      server->ssh.port                  = 22;
      server->ssh.loginName             = NULL;
      server->ssh.password              = NULL;
      initKey(&server->ssh.publicKey);
      initKey(&server->ssh.privateKey);
      break;
    case SERVER_TYPE_WEBDAV:
      server->webDAV.loginName          = NULL;
      server->webDAV.password           = NULL;
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
      if (server->ftp.password != NULL) Password_delete(server->ftp.password);
      if (server->ftp.loginName != NULL) String_delete(server->ftp.loginName);
      break;
    case SERVER_TYPE_SSH:
      if (isKeyAvailable(&server->ssh.privateKey)) doneKey(&server->ssh.privateKey);
      if (isKeyAvailable(&server->ssh.publicKey)) doneKey(&server->ssh.publicKey);
      if (server->ssh.password != NULL) Password_delete(server->ssh.password);
      if (server->ssh.loginName != NULL) String_delete(server->ssh.loginName);
      break;
    case SERVER_TYPE_WEBDAV:
      if (isKeyAvailable(&server->webDAV.privateKey)) doneKey(&server->webDAV.privateKey);
      if (isKeyAvailable(&server->webDAV.publicKey)) doneKey(&server->webDAV.publicKey);
      if (server->webDAV.password != NULL) Password_delete(server->webDAV.password);
      if (server->webDAV.loginName != NULL) String_delete(server->webDAV.loginName);
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
  initServer(&serverNode->server,name,serverType);
  serverNode->id                                  = Misc_getId();
//  serverNode->name = String_duplicate(name);
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

uint getServerSettings(const StorageSpecifier *storageSpecifier,
                       const JobOptions       *jobOptions,
                       Server                 *server
                      )
{
  uint          serverId;
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;

  assert(storageSpecifier != NULL);
  assert(server != NULL);

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
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand ) ? serverNode->server.writePreProcessCommand  : globalOptions.file.writePreProcessCommand );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand) ? serverNode->server.writePostProcessCommand : globalOptions.file.writePostProcessCommand);
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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
          server->ftp.password  = Password_duplicate(serverNode->server.ftp.password);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand ) ? serverNode->server.writePreProcessCommand  : globalOptions.ftp.writePreProcessCommand );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand) ? serverNode->server.writePostProcessCommand : globalOptions.ftp.writePostProcessCommand);
        }
      }
      break;
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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
          server->ssh.password  = Password_duplicate(serverNode->server.ssh.password);
          duplicateKey(&server->ssh.publicKey,&serverNode->server.ssh.publicKey);
          duplicateKey(&server->ssh.privateKey,&serverNode->server.ssh.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand ) ? serverNode->server.writePreProcessCommand  : globalOptions.scp.writePreProcessCommand );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand) ? serverNode->server.writePostProcessCommand : globalOptions.scp.writePostProcessCommand);
        }
      }
      break;
    case STORAGE_TYPE_SFTP:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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
          server->ssh.password  = Password_duplicate(serverNode->server.ssh.password);
          duplicateKey(&server->ssh.publicKey,&serverNode->server.ssh.publicKey);
          duplicateKey(&server->ssh.privateKey,&serverNode->server.ssh.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand ) ? serverNode->server.writePreProcessCommand  : globalOptions.sftp.writePreProcessCommand );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand) ? serverNode->server.writePostProcessCommand : globalOptions.sftp.writePostProcessCommand);
        }
      }
      break;
    case STORAGE_TYPE_WEBDAV:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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
          server->webDAV.password  = Password_duplicate(serverNode->server.webDAV.password);
          duplicateKey(&server->webDAV.publicKey,&serverNode->server.webDAV.publicKey);
          duplicateKey(&server->webDAV.privateKey,&serverNode->server.webDAV.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(serverNode->server.writePreProcessCommand ) ? serverNode->server.writePreProcessCommand  : globalOptions.webdav.writePreProcessCommand );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(serverNode->server.writePostProcessCommand) ? serverNode->server.writePostProcessCommand : globalOptions.webdav.writePostProcessCommand);
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

uint getFileServerSettings(ConstString      directory,
                           const JobOptions *jobOptions,
                           FileServer       *fileServer
                          )
{
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;

  assert(directory != NULL);
  assert(fileServer != NULL);

  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(fileServer);

  serverNode = NULL;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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

uint getFTPServerSettings(ConstString      hostName,
                          const JobOptions *jobOptions,
                          FTPServer        *ftpServer
                         )
{
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;

  assert(hostName != NULL);
  assert(ftpServer != NULL);

  serverNode = NULL;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    // find FTP server
    serverNode = LIST_FIND(&globalOptions.serverList,
                           serverNode,
                              (serverNode->server.type == SERVER_TYPE_FTP)
                           && String_equals(serverNode->server.name,hostName)
                          );

    // get FTP server settings
    ftpServer->loginName               = ((jobOptions != NULL) && !String_isEmpty(jobOptions->ftpServer.loginName)           ) ? jobOptions->ftpServer.loginName               : ((serverNode != NULL) ? serverNode->server.ftp.loginName               : globalOptions.defaultFTPServer->ftp.loginName              );
    ftpServer->password                = ((jobOptions != NULL) && !Password_isEmpty(jobOptions->ftpServer.password)          ) ? jobOptions->ftpServer.password                : ((serverNode != NULL) ? serverNode->server.ftp.password                : globalOptions.defaultFTPServer->ftp.password               );
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

uint getSSHServerSettings(ConstString      hostName,
                          const JobOptions *jobOptions,
                          SSHServer        *sshServer
                         )
{
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;

  assert(hostName != NULL);
  assert(sshServer != NULL);

  serverNode = NULL;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    // find SSH server
    serverNode = LIST_FIND(&globalOptions.serverList,
                           serverNode,
                              (serverNode->server.type == SERVER_TYPE_SSH)
                           && String_equals(serverNode->server.name,hostName)
                          );

    // get SSH server settings
    sshServer->port                    = ((jobOptions != NULL) && (jobOptions->sshServer.port != 0)                             ) ? jobOptions->sshServer.port                    : ((serverNode != NULL) ? serverNode->server.ssh.port                    : globalOptions.defaultSSHServer->ssh.port                   );
    sshServer->loginName               = ((jobOptions != NULL) && !String_isEmpty(jobOptions->sshServer.loginName)              ) ? jobOptions->sshServer.loginName               : ((serverNode != NULL) ? serverNode->server.ssh.loginName               : globalOptions.defaultSSHServer->ssh.loginName              );
    sshServer->password                = ((jobOptions != NULL) && !Password_isEmpty(jobOptions->sshServer.password)             ) ? jobOptions->sshServer.password                : ((serverNode != NULL) ? serverNode->server.ssh.password                : globalOptions.defaultSSHServer->ssh.password               );
    sshServer->publicKey               = ((jobOptions != NULL) && isKeyAvailable(&jobOptions->sshServer.publicKey)              ) ? jobOptions->sshServer.publicKey               : ((serverNode != NULL) ? serverNode->server.ssh.publicKey               : globalOptions.defaultSSHServer->ssh.publicKey              );
    sshServer->privateKey              = ((jobOptions != NULL) && isKeyAvailable(&jobOptions->sshServer.privateKey)             ) ? jobOptions->sshServer.privateKey              : ((serverNode != NULL) ? serverNode->server.ssh.privateKey              : globalOptions.defaultSSHServer->ssh.privateKey             );
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

uint getWebDAVServerSettings(ConstString      hostName,
                             const JobOptions *jobOptions,
                             WebDAVServer     *webDAVServer
                            )
{
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;

  assert(hostName != NULL);
  assert(webDAVServer != NULL);

  serverNode = NULL;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    // find WebDAV server
    serverNode = LIST_FIND(&globalOptions.serverList,
                           serverNode,
                              (serverNode->server.type == SERVER_TYPE_WEBDAV)
                           && String_equals(serverNode->server.name,hostName)
                          );

    // get WebDAV server settings
//    webDAVServer->port                    = ((jobOptions != NULL) && (jobOptions->webDAVServer.port != 0) ? jobOptions->webDAVServer.port : ((serverNode != NULL) ? serverNode->webDAVServer.port : globalOptions.defaultWebDAVServer->port );
    webDAVServer->loginName               = ((jobOptions != NULL) && !String_isEmpty(jobOptions->webDAVServer.loginName)              ) ? jobOptions->webDAVServer.loginName               : ((serverNode != NULL) ? serverNode->server.webDAV.loginName               : globalOptions.defaultWebDAVServer->webDAV.loginName              );
    webDAVServer->password                = ((jobOptions != NULL) && !Password_isEmpty(jobOptions->webDAVServer.password)             ) ? jobOptions->webDAVServer.password                : ((serverNode != NULL) ? serverNode->server.webDAV.password                : globalOptions.defaultWebDAVServer->webDAV.password               );
    webDAVServer->publicKey               = ((jobOptions != NULL) && isKeyAvailable(&jobOptions->webDAVServer.publicKey)              ) ? jobOptions->webDAVServer.publicKey               : ((serverNode != NULL) ? serverNode->server.webDAV.publicKey               : globalOptions.defaultWebDAVServer->webDAV.publicKey              );
    webDAVServer->privateKey              = ((jobOptions != NULL) && isKeyAvailable(&jobOptions->webDAVServer.privateKey)             ) ? jobOptions->webDAVServer.privateKey              : ((serverNode != NULL) ? serverNode->server.webDAV.privateKey              : globalOptions.defaultWebDAVServer->webDAV.privateKey             );
  }

  return (serverNode != NULL) ? serverNode->id : 0;
}

void getCDSettings(const JobOptions *jobOptions,
                   OpticalDisk      *opticalDisk
                  )
{
  assert(opticalDisk != NULL);

  opticalDisk->deviceName              = ((jobOptions != NULL) && (jobOptions->opticalDisk.deviceName              != NULL)) ? jobOptions->opticalDisk.deviceName              : globalOptions.cd.deviceName;
  opticalDisk->requestVolumeCommand    = ((jobOptions != NULL) && (jobOptions->opticalDisk.requestVolumeCommand    != NULL)) ? jobOptions->opticalDisk.requestVolumeCommand    : globalOptions.cd.requestVolumeCommand;
  opticalDisk->unloadVolumeCommand     = ((jobOptions != NULL) && (jobOptions->opticalDisk.unloadVolumeCommand     != NULL)) ? jobOptions->opticalDisk.unloadVolumeCommand     : globalOptions.cd.unloadVolumeCommand;
  opticalDisk->loadVolumeCommand       = ((jobOptions != NULL) && (jobOptions->opticalDisk.loadVolumeCommand       != NULL)) ? jobOptions->opticalDisk.loadVolumeCommand       : globalOptions.cd.loadVolumeCommand;
  opticalDisk->volumeSize              = ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize              != 0LL )) ? jobOptions->opticalDisk.volumeSize              : globalOptions.cd.volumeSize;
  opticalDisk->imagePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->opticalDisk.imagePreProcessCommand  != NULL)) ? jobOptions->opticalDisk.imagePreProcessCommand  : globalOptions.cd.imagePreProcessCommand;
  opticalDisk->imagePostProcessCommand = ((jobOptions != NULL) && (jobOptions->opticalDisk.imagePostProcessCommand != NULL)) ? jobOptions->opticalDisk.imagePostProcessCommand : globalOptions.cd.imagePostProcessCommand;
  opticalDisk->imageCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.imageCommand            != NULL)) ? jobOptions->opticalDisk.imageCommand            : globalOptions.cd.imageCommand;
  opticalDisk->eccPreProcessCommand    = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccPreProcessCommand    != NULL)) ? jobOptions->opticalDisk.eccPreProcessCommand    : globalOptions.cd.eccPreProcessCommand;
  opticalDisk->eccPostProcessCommand   = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccPostProcessCommand   != NULL)) ? jobOptions->opticalDisk.eccPostProcessCommand   : globalOptions.cd.eccPostProcessCommand;
  opticalDisk->eccCommand              = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccCommand              != NULL)) ? jobOptions->opticalDisk.eccCommand              : globalOptions.cd.eccCommand;
  opticalDisk->blankCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccCommand              != NULL)) ? jobOptions->opticalDisk.blankCommand            : globalOptions.cd.blankCommand;
  opticalDisk->writePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->opticalDisk.writePreProcessCommand  != NULL)) ? jobOptions->opticalDisk.writePreProcessCommand  : globalOptions.cd.writePreProcessCommand;
  opticalDisk->writePostProcessCommand = ((jobOptions != NULL) && (jobOptions->opticalDisk.writePostProcessCommand != NULL)) ? jobOptions->opticalDisk.writePostProcessCommand : globalOptions.cd.writePostProcessCommand;
  opticalDisk->writeCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.writeCommand            != NULL)) ? jobOptions->opticalDisk.writeCommand            : globalOptions.cd.writeCommand;
  opticalDisk->writeImageCommand       = ((jobOptions != NULL) && (jobOptions->opticalDisk.writeImageCommand       != NULL)) ? jobOptions->opticalDisk.writeImageCommand       : globalOptions.cd.writeImageCommand;
}

void getDVDSettings(const JobOptions *jobOptions,
                    OpticalDisk      *opticalDisk
                   )
{
  assert(opticalDisk != NULL);

  opticalDisk->deviceName              = ((jobOptions != NULL) && (jobOptions->opticalDisk.deviceName              != NULL)) ? jobOptions->opticalDisk.deviceName              : globalOptions.dvd.deviceName;
  opticalDisk->requestVolumeCommand    = ((jobOptions != NULL) && (jobOptions->opticalDisk.requestVolumeCommand    != NULL)) ? jobOptions->opticalDisk.requestVolumeCommand    : globalOptions.dvd.requestVolumeCommand;
  opticalDisk->unloadVolumeCommand     = ((jobOptions != NULL) && (jobOptions->opticalDisk.unloadVolumeCommand     != NULL)) ? jobOptions->opticalDisk.unloadVolumeCommand     : globalOptions.dvd.unloadVolumeCommand;
  opticalDisk->loadVolumeCommand       = ((jobOptions != NULL) && (jobOptions->opticalDisk.loadVolumeCommand       != NULL)) ? jobOptions->opticalDisk.loadVolumeCommand       : globalOptions.dvd.loadVolumeCommand;
  opticalDisk->volumeSize              = ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize              != 0LL )) ? jobOptions->opticalDisk.volumeSize              : globalOptions.dvd.volumeSize;
  opticalDisk->imagePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->opticalDisk.imagePreProcessCommand  != NULL)) ? jobOptions->opticalDisk.imagePreProcessCommand  : globalOptions.dvd.imagePreProcessCommand;
  opticalDisk->imagePostProcessCommand = ((jobOptions != NULL) && (jobOptions->opticalDisk.imagePostProcessCommand != NULL)) ? jobOptions->opticalDisk.imagePostProcessCommand : globalOptions.dvd.imagePostProcessCommand;
  opticalDisk->imageCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.imageCommand            != NULL)) ? jobOptions->opticalDisk.imageCommand            : globalOptions.dvd.imageCommand;
  opticalDisk->eccPreProcessCommand    = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccPreProcessCommand    != NULL)) ? jobOptions->opticalDisk.eccPreProcessCommand    : globalOptions.dvd.eccPreProcessCommand;
  opticalDisk->eccPostProcessCommand   = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccPostProcessCommand   != NULL)) ? jobOptions->opticalDisk.eccPostProcessCommand   : globalOptions.dvd.eccPostProcessCommand;
  opticalDisk->eccCommand              = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccCommand              != NULL)) ? jobOptions->opticalDisk.eccCommand              : globalOptions.dvd.eccCommand;
  opticalDisk->blankCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccCommand              != NULL)) ? jobOptions->opticalDisk.blankCommand            : globalOptions.dvd.blankCommand;
  opticalDisk->writePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->opticalDisk.writePreProcessCommand  != NULL)) ? jobOptions->opticalDisk.writePreProcessCommand  : globalOptions.dvd.writePreProcessCommand;
  opticalDisk->writePostProcessCommand = ((jobOptions != NULL) && (jobOptions->opticalDisk.writePostProcessCommand != NULL)) ? jobOptions->opticalDisk.writePostProcessCommand : globalOptions.dvd.writePostProcessCommand;
  opticalDisk->writeCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.writeCommand            != NULL)) ? jobOptions->opticalDisk.writeCommand            : globalOptions.dvd.writeCommand;
  opticalDisk->writeImageCommand       = ((jobOptions != NULL) && (jobOptions->opticalDisk.writeImageCommand       != NULL)) ? jobOptions->opticalDisk.writeImageCommand       : globalOptions.dvd.writeImageCommand;
}

void getBDSettings(const JobOptions *jobOptions,
                   OpticalDisk      *opticalDisk
                  )
{
  assert(opticalDisk != NULL);

  opticalDisk->deviceName              = ((jobOptions != NULL) && (jobOptions->opticalDisk.deviceName              != NULL)) ? jobOptions->opticalDisk.deviceName              : globalOptions.bd.deviceName;
  opticalDisk->requestVolumeCommand    = ((jobOptions != NULL) && (jobOptions->opticalDisk.requestVolumeCommand    != NULL)) ? jobOptions->opticalDisk.requestVolumeCommand    : globalOptions.bd.requestVolumeCommand;
  opticalDisk->unloadVolumeCommand     = ((jobOptions != NULL) && (jobOptions->opticalDisk.unloadVolumeCommand     != NULL)) ? jobOptions->opticalDisk.unloadVolumeCommand     : globalOptions.bd.unloadVolumeCommand;
  opticalDisk->loadVolumeCommand       = ((jobOptions != NULL) && (jobOptions->opticalDisk.loadVolumeCommand       != NULL)) ? jobOptions->opticalDisk.loadVolumeCommand       : globalOptions.bd.loadVolumeCommand;
  opticalDisk->volumeSize              = ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize              != 0LL )) ? jobOptions->opticalDisk.volumeSize              : globalOptions.bd.volumeSize;
  opticalDisk->imagePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->opticalDisk.imagePreProcessCommand  != NULL)) ? jobOptions->opticalDisk.imagePreProcessCommand  : globalOptions.bd.imagePreProcessCommand;
  opticalDisk->imagePostProcessCommand = ((jobOptions != NULL) && (jobOptions->opticalDisk.imagePostProcessCommand != NULL)) ? jobOptions->opticalDisk.imagePostProcessCommand : globalOptions.bd.imagePostProcessCommand;
  opticalDisk->imageCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.imageCommand            != NULL)) ? jobOptions->opticalDisk.imageCommand            : globalOptions.bd.imageCommand;
  opticalDisk->eccPreProcessCommand    = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccPreProcessCommand    != NULL)) ? jobOptions->opticalDisk.eccPreProcessCommand    : globalOptions.bd.eccPreProcessCommand;
  opticalDisk->eccPostProcessCommand   = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccPostProcessCommand   != NULL)) ? jobOptions->opticalDisk.eccPostProcessCommand   : globalOptions.bd.eccPostProcessCommand;
  opticalDisk->eccCommand              = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccCommand              != NULL)) ? jobOptions->opticalDisk.eccCommand              : globalOptions.bd.eccCommand;
  opticalDisk->blankCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.eccCommand              != NULL)) ? jobOptions->opticalDisk.blankCommand            : globalOptions.bd.blankCommand;
  opticalDisk->writePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->opticalDisk.writePreProcessCommand  != NULL)) ? jobOptions->opticalDisk.writePreProcessCommand  : globalOptions.bd.writePreProcessCommand;
  opticalDisk->writePostProcessCommand = ((jobOptions != NULL) && (jobOptions->opticalDisk.writePostProcessCommand != NULL)) ? jobOptions->opticalDisk.writePostProcessCommand : globalOptions.bd.writePostProcessCommand;
  opticalDisk->writeCommand            = ((jobOptions != NULL) && (jobOptions->opticalDisk.writeCommand            != NULL)) ? jobOptions->opticalDisk.writeCommand            : globalOptions.bd.writeCommand;
  opticalDisk->writeImageCommand       = ((jobOptions != NULL) && (jobOptions->opticalDisk.writeImageCommand       != NULL)) ? jobOptions->opticalDisk.writeImageCommand       : globalOptions.bd.writeImageCommand;
}

void getDeviceSettings(ConstString      name,
                       const JobOptions *jobOptions,
                       Device           *device
                      )
{
  SemaphoreLock semaphoreLock;
  DeviceNode    *deviceNode;

  assert(name != NULL);
  assert(device != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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

bool allocateServer(uint serverId, ServerConnectionPriorities priority, long timeout)
{
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;
  uint          maxConnectionCount;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
            Semaphore_signalModified(&globalOptions.serverList.lock);

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
            Semaphore_signalModified(&globalOptions.serverList.lock);

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
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
  bool          pendingFlag;
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;

  pendingFlag = FALSE;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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

MountNode *newMountNode(ConstString mountName, ConstString deviceName, bool alwaysUnmount)
{
  assert(mountName != NULL);

  return newMountNodeCString(String_cString(mountName),
                             String_cString(deviceName),
                             alwaysUnmount
                            );
}

MountNode *newMountNodeCString(const char *mountName, const char *deviceName, bool alwaysUnmount)
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
  mountNode->alwaysUnmount = alwaysUnmount;
  mountNode->mounted       = FALSE;

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
                           fromMountNode->device,
                           fromMountNode->alwaysUnmount
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

Errors getPasswordConsole(String        name,
                          Password      *password,
                          PasswordTypes passwordType,
                          const char    *text,
                          bool          validateFlag,
                          bool          weakCheckFlag,
                          void          *userData
                         )
{
  Errors        error;
  SemaphoreLock semaphoreLock;

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

        SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          saveConsole(stdout,&saveLine);

          // input password
          title = String_new();
          switch (passwordType)
          {
            case PASSWORD_TYPE_CRYPT : String_format(title,"Crypt"); break;
            case PASSWORD_TYPE_FTP   : String_format(title,"FTP"); break;
            case PASSWORD_TYPE_SSH   : String_format(title,"SSH"); break;
            case PASSWORD_TYPE_WEBDAV: String_format(title,"WebDAV"); break;
          }
          if (!stringIsEmpty(text))
          {
            String_format(title," password for '%s'",text);
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
              String_format(String_clear(title),"Verify password for '%s'",text);
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
              printError("Crypt passwords are not equal!\n");
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
              printWarning("Low password quality!\n");
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
      return FALSE;
    }
  }
  String_delete(name);

  return TRUE;
}

bool configValueParsePassword(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if ((*(Password**)variable) == NULL)
  {
    (*(Password**)variable) = Password_new();
  }
  Password_setCString(*(Password**)variable,value);

  return TRUE;
}

void configValueFormatInitPassord(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (*(Password**)variable);
}

void configValueFormatDonePassword(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatPassword(void **formatUserData, void *userData, String line)
{
  Password   *password;
  const char *plain;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  password = (Password*)(*formatUserData);
  if (password != NULL)
  {
    plain = Password_deploy(password);
    String_format(line,"%'s",plain);
    Password_undeploy(password,plain);

    (*formatUserData) = NULL;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

#ifdef MULTI_CRYPT
bool configValueParseCryptAlgorithms(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  CryptAlgorithms cryptAlgorithms[4];
  uint            i;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

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
    if (!Crypt_parseAlgorithm(String_cString(token),&((CryptAlgorithms*)variable)[i]))
    {
      stringFormat(errorMessage,errorMessageSize,"Unknown crypt algorithm '%s'",String_cString(token));
      String_doneTokenizer(&stringTokenizer);
      return FALSE;
    }
    if (((CryptAlgorithms*)variable)[i] != CRYPT_ALGORITHM_NONE)
    {
      i++;
    }
  }
  String_doneTokenizer(&stringTokenizer);
  while (i < 4)
  {
    ((CryptAlgorithms*)variable)[i] = CRYPT_ALGORITHM_NONE;
    i++;
  }

  return TRUE;
}
#endif /* MULTI_CRYPT */

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
  uint            i;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  cryptAlgorithms = (CryptAlgorithms*)(*formatUserData);
  if (cryptAlgorithms != NULL)
  {
    i = 0;
    while ((i < 4) && (cryptAlgorithms[i] != CRYPT_ALGORITHM_NONE))
    {
      if (i > 0) String_appendChar(line,'+');
      String_appendCString(line,Crypt_algorithmToString(cryptAlgorithms[i],NULL));
      i++;
    }

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
  bandWidthNode = parseBandWidth(s);
  if (bandWidthNode == NULL)
  {
    stringFormat(errorMessage,errorMessageSize,"Cannot parse bandwidth value '%s'",value);
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
  if (bandWidthNode->fileName != NULL)
  {
    String_format(line,"%s",bandWidthNode->fileName);
  }
  else
  {
    String_format(line,"%lu",bandWidthNode->n);
  }

  return TRUE;
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
    userId  = File_userNameToUserId(userName);
    groupId = File_groupNameToGroupId(groupName);
  }
  else if (String_scanCString(value,"%256s:",userName))
  {
    userId  = File_userNameToUserId(userName);
    groupId = FILE_DEFAULT_GROUP_ID;
  }
  else if (String_scanCString(value,":%256s",groupName))
  {
    userId  = FILE_DEFAULT_USER_ID;
    groupId = File_groupNameToGroupId(groupName);
  }
  else if (String_scanCString(value,"%256s",userName))
  {
    userId  = File_userNameToUserId(userName);
    groupId = FILE_DEFAULT_GROUP_ID;
  }
  else
  {
    stringFormat(errorMessage,errorMessageSize,"Unknown owner ship value '%s'",value);
    return FALSE;
  }

  // store owner values
  ((JobOptionsOwner*)variable)->userId  = userId;
  ((JobOptionsOwner*)variable)->groupId = groupId;

  return TRUE;
}

void configValueFormatInitOwner(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (JobOptionsOwner*)variable;
}

void configValueFormatDoneOwner(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatOwner(void **formatUserData, void *userData, String line)
{
  JobOptionsOwner *jobOptionsOwner;
  char            userName[256],groupName[256];

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  jobOptionsOwner = (JobOptionsOwner*)(*formatUserData);
  if (jobOptionsOwner != NULL)
  {
    if (File_userIdToUserName  (userName, sizeof(userName), jobOptionsOwner->userId )) return FALSE;
    if (File_groupIdToGroupName(groupName,sizeof(groupName),jobOptionsOwner->groupId)) return FALSE;
    String_format(line,"%s:%s",userName,groupName);

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
  String       pattern;
  Errors       error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
//??? userData = default patterType?
  UNUSED_VARIABLE(name);

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // append to list
#ifndef WERROR
#warning TODO String_mapCString needed?
#endif
  pattern = String_mapCString(String_newCString(value),STRING_BEGIN,FILENAME_MAP_FROM,FILENAME_MAP_TO,SIZE_OF_ARRAY(FILENAME_MAP_FROM));
  error = EntryList_append((EntryList*)variable,entryType,pattern,patternType,NULL);
  if (error != ERROR_NONE)
  {
    strncpy(errorMessage,Error_getText(error),errorMessageSize); errorMessage[errorMessageSize-1] = '\0';
    String_delete(pattern);
    return FALSE;
  }
  String_delete(pattern);

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
        String_format(line,"%'S",fileName);
        break;
      case PATTERN_TYPE_REGEX:
        String_format(line,"r:%'S",fileName);
        break;
      case PATTERN_TYPE_EXTENDED_REGEX:
        String_format(line,"x:%'S",fileName);
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
        String_format(line,"%'S",entryNode->string);
        break;
      case PATTERN_TYPE_REGEX:
        String_format(line,"r:%'S",entryNode->string);
        break;
      case PATTERN_TYPE_EXTENDED_REGEX:
        String_format(line,"x:%'S",entryNode->string);
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

  // append to list
  error = PatternList_appendCString((PatternList*)variable,value,patternType,NULL);
  if (error != ERROR_NONE)
  {
    strncpy(errorMessage,Error_getText(error),errorMessageSize); errorMessage[errorMessageSize-1] = '\0';
    return FALSE;
  }

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
        String_format(line,"%'S",patternNode->string);
        break;
      case PATTERN_TYPE_REGEX:
        String_format(line,"r:%'S",patternNode->string);
        break;
      case PATTERN_TYPE_EXTENDED_REGEX:
        String_format(line,"x:%'S",patternNode->string);
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
  bool      alwaysUnmount;
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // init variables
  mountName     = String_new();
  alwaysUnmount = FALSE;

  // get name, alwaysUnmount
  if      (String_parseCString(value,"%S,%y",NULL,mountName,&alwaysUnmount))
  {
  }
  else if (String_parseCString(value,"%S",NULL,mountName))
  {
    alwaysUnmount = FALSE;
  }
  else
  {
    String_delete(mountName);
    return FALSE;
  }

  if (!String_isEmpty(mountName))
  {
    // add to mount list
    mountNode = newMountNode(mountName,
                             NULL,  // deviceName
                             alwaysUnmount
                            );
    assert(mountNode != NULL);
    List_append((MountList*)variable,mountNode);
  }

  // free resources
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
    String_format(line,"%'S,%y",mountNode->name,mountNode->alwaysUnmount);

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

  // append to delta source list
  deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
  if (deltaSourceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  deltaSourceNode->storageName = String_newCString(value);
  deltaSourceNode->patternType = patternType;
  List_append((DeltaSourceList*)variable,deltaSourceNode);

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
        String_format(line,"%'S",deltaSourceNode->storageName);
        break;
      case PATTERN_TYPE_REGEX:
        String_format(line,"r:%'S",deltaSourceNode->storageName);
        break;
      case PATTERN_TYPE_EXTENDED_REGEX:
        String_format(line,"x:%'S",deltaSourceNode->storageName);
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

bool configValueParseString(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
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
  ((JobOptionsCompressAlgorithms*)variable)->delta = compressAlgorithmDelta;
  ((JobOptionsCompressAlgorithms*)variable)->byte  = compressAlgorithmByte;

  return TRUE;
}

void configValueFormatInitCompressAlgorithms(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (JobOptionsCompressAlgorithms*)variable;
}

void configValueFormatDoneCompressAlgorithms(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatCompressAlgorithms(void **formatUserData, void *userData, String line)
{
  JobOptionsCompressAlgorithms *jobOptionsCompressAlgorithms;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  jobOptionsCompressAlgorithms = (JobOptionsCompressAlgorithms*)(*formatUserData);
  if (jobOptionsCompressAlgorithms != NULL)
  {
    String_format(line,
                  "%s+%s",
                  CmdOption_selectToString(COMPRESS_ALGORITHMS_DELTA,jobOptionsCompressAlgorithms->delta,NULL),
                  CmdOption_selectToString(COMPRESS_ALGORITHMS_BYTE, jobOptionsCompressAlgorithms->byte, NULL)
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
      if (!Misc_base64DecodeCString((byte*)data,dataLength,&value[7]))
      {
        Password_freeSecure(data);
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
    data = Password_allocSecure((size_t)dataLength);
    if (data == NULL)
    {
      (void)File_close(&fileHandle);
      return ERROR_INSUFFICIENT_MEMORY;
    }

    // decode base64
    if (!Misc_base64Decode((byte*)data,dataLength,string,STRING_BEGIN))
    {
      Password_freeSecure(data);
      return FALSE;
    }

    // set key data
    if (key->data != NULL) Password_freeSecure(key->data);
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
      data = Password_allocSecure(dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // decode base64
      if (!Misc_base64DecodeCString((byte*)data,dataLength,&value[7]))
      {
        Password_freeSecure(data);
        return FALSE;
      }

      // set key data
      if (key->data != NULL) Password_freeSecure(key->data);
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
      data = Password_allocSecure(dataLength);
      if (data == NULL)
      {
        return FALSE;
      }

      // copy data
      memcpy(data,value,dataLength);

      // set key data
      if (key->data != NULL) Password_freeSecure(key->data);
      key->data   = data;
      key->length = dataLength;
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

  key = (Key*)(*formatUserData);
  if (key != NULL)
  {
    String_appendCString(line,"base64:");
    Misc_base64Encode(line,key->data,key->length);
    (*formatUserData) = NULL;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool configValueParseDeprecatedMountDevice(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (!stringIsEmpty(value))
  {
    // add to mount list
    mountNode = newMountNodeCString(value,
                                    NULL,  // deviceName
                                    TRUE  // alwaysUnmount
                                   );
    assert(mountNode != NULL);
    List_append((MountList*)variable,mountNode);
  }

  return TRUE;
}

bool configValueParseDeprecatedStopOnError(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(bool*)variable) = !(   (value == NULL)
                         || stringEquals(value,"1")
                         || stringEqualsIgnoreCase(value,"true")
                         || stringEqualsIgnoreCase(value,"on")
                         || stringEqualsIgnoreCase(value,"yes")
                        );

  return TRUE;
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

Errors addIncludeListCommand(EntryTypes entryType, EntryList *entryList, const char *template)
{
  String     script;
//  TextMacro textMacros[5];
  Errors error;

  assert(entryList != NULL);
  assert(template != NULL);

  // init variables
  script = String_new();

  // expand template
//  TEXT_MACRO_N_STRING (textMacros[1],"%name",jobName,                             TEXT_MACRO_PATTERN_STRING);
//  TEXT_MACRO_N_CSTRING(textMacros[2],"%type",getArchiveTypeName(archiveType),     TEXT_MACRO_PATTERN_STRING);
//  TEXT_MACRO_N_CSTRING(textMacros[3],"%T",   getArchiveTypeShortName(archiveType),".");
//  TEXT_MACRO_N_STRING (textMacros[4],"%text",scheduleCustomText,                  TEXT_MACRO_PATTERN_STRING);
  Misc_expandMacros(script,
                    template,
                    EXPAND_MACRO_MODE_STRING,
NULL,0,//                    textMacros,SIZE_OF_ARRAY(textMacros),
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               EntryList_append(entryList,entryType,line,PATTERN_TYPE_GLOB,NULL);
                             },NULL),
                             CALLBACK(NULL,NULL)
                            );
  if (error != ERROR_NONE)
  {
    printWarning("Execute command for '%s' failed: %s\n",template,Error_getText(error));
    String_delete(script);
    return error;
  }

  // free resources
  String_delete(script);

  return ERROR_NONE;
}

Errors addExcludeListCommand(PatternList *patternList, const char *template)
{
  String     script;
//  TextMacro textMacros[5];
  Errors error;

  assert(patternList != NULL);
  assert(template != NULL);

  // init variables
  script = String_new();

  // expand template
//  TEXT_MACRO_N_STRING (textMacros[1],"%name",jobName,                             TEXT_MACRO_PATTERN_STRING);
//  TEXT_MACRO_N_CSTRING(textMacros[2],"%type",getArchiveTypeName(archiveType),     TEXT_MACRO_PATTERN_STRING);
//  TEXT_MACRO_N_CSTRING(textMacros[3],"%T",   getArchiveTypeShortName(archiveType),".");
//  TEXT_MACRO_N_STRING (textMacros[4],"%text",scheduleCustomText,                  TEXT_MACRO_PATTERN_STRING);
  Misc_expandMacros(script,
                    template,
                    EXPAND_MACRO_MODE_STRING,
NULL,0,//                    textMacros,SIZE_OF_ARRAY(textMacros),
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               PatternList_append(patternList,line,PATTERN_TYPE_GLOB,NULL);
                             },NULL),
                             CALLBACK(NULL,NULL)
                            );
  if (error != ERROR_NONE)
  {
    printWarning("Execute command for '%s' failed: %s\n",template,Error_getText(error));
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
                      ConstString     name
                     )
{
  assert(excludePatternList != NULL);
  assert(name != NULL);

  return PatternList_match(excludePatternList,name,PATTERN_MATCH_MODE_EXACT);
}

bool isNoBackup(ConstString pathName)
{
  String fileName;
  bool   haveNoBackupFlag;

  assert(pathName != NULL);

  haveNoBackupFlag = FALSE;
  if (!globalOptions.ignoreNoBackupFileFlag)
  {
    fileName = String_new();
    haveNoBackupFlag |= File_exists(File_appendFileNameCString(File_setFileName(fileName,pathName),".nobackup"));
    haveNoBackupFlag |= File_exists(File_appendFileNameCString(File_setFileName(fileName,pathName),".NOBACKUP"));
    String_delete(fileName);
  }

  return haveNoBackupFlag;
}

// ----------------------------------------------------------------------

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
  String     name,value;
  long       nextIndex;

  assert(fileName != NULL);

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open job file '%s' (error: %s)!\n",
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
      String_unquote(value,STRING_QUOTES);
      if (!ConfigValue_parse(String_cString(name),
                             String_cString(value),
                             CONFIG_VALUES,
                             NULL, // sectionName,
                             NULL, // outputHandle,
                             NULL, // errorPrefix,
                             NULL, // warningPrefix
                             NULL  // variable
                            )
         )
      {
        printError("Unknown or invalid config value '%s' in %s, line %ld - skipped\n",
                   String_cString(name),
                   String_cString(fileName),
                   lineNb
                  );
      }
    }
    else
    {
      printError("Syntax error in '%s', line %ld: '%s' - skipped\n",
                 String_cString(fileName),
                 lineNb,
                 String_cString(line)
                );
    }
  }
  String_delete(value);
  String_delete(name);
  String_delete(title);
  String_delete(line);

  // close file
  (void)File_close(&fileHandle);

  return !failFlag;
}

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
      printError("Cannot create process id file '%s' (error: %s)\n",pidFileName,Error_getText(error));
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
    File_deleteCString(pidFileName,FALSE);
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
    case ERROR_NONE:
      return EXITCODE_OK;
      break;
    case ERROR_TESTCODE:
      return EXITCODE_TESTCODE;
      break;
    case ERROR_INVALID_ARGUMENT:
      return EXITCODE_INVALID_ARGUMENT;
      break;
    case ERROR_CONFIG:
      return EXITCODE_CONFIG_ERROR;
    case ERROR_FUNCTION_NOT_SUPPORTED:
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
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors generateEncryptionKeys(const char *keyFileBaseName)
{
  String   publicKeyFileName,privateKeyFileName;
  String   data;
  Errors   error;
  Password cryptPassword;
  CryptKey publicKey,privateKey;
  String   pathName;

  // initialize variables
  publicKeyFileName  = String_new();
  privateKeyFileName = String_new();
  data               = String_new();
  Password_init(&cryptPassword);

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
      printError("Public key file '%s' already exists!\n",String_cString(publicKeyFileName));
      Password_done(&cryptPassword);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
    if (File_exists(privateKeyFileName))
    {
      printError("Private key file '%s' already exists!\n",String_cString(privateKeyFileName));
      Password_done(&cryptPassword);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
  }

  // get crypt password for private key encryption
  if (Password_isEmpty(globalOptions.cryptPassword))
  {
    error = getPasswordConsole(NULL,  // name
                               &cryptPassword,
                               PASSWORD_TYPE_CRYPT,
                               String_cString(privateKeyFileName),
                               TRUE,  // validateFlag
                               FALSE, // weakCheckFlag
                               NULL  // userData
                              );
    if (error != ERROR_NONE)
    {
      printError("No password given for private key!\n");
      Password_done(&cryptPassword);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
  }
  else
  {
    Password_set(&cryptPassword,globalOptions.cryptPassword);
  }

  // generate new key pair for encryption
  error = Crypt_createPublicPrivateKeyPair(&publicKey,&privateKey,keyBits,CRYPT_PADDING_TYPE_NONE,CRYPT_KEY_MODE_NONE);
  if (error != ERROR_NONE)
  {
    printError("Cannot create encryption key pair (error: %s)!\n",Error_getText(error));
    Password_done(&cryptPassword);
    String_delete(data);
    String_delete(privateKeyFileName);
    String_delete(publicKeyFileName);
    return error;
  }
//fprintf(stderr,"%s, %d: public %d \n",__FILE__,__LINE__,publicKey.dataLength); debugDumpMemory(publicKey.data,publicKey.dataLength,0);
//fprintf(stderr,"%s, %d: private %d\n",__FILE__,__LINE__,privateKey.dataLength); debugDumpMemory(privateKey.data,privateKey.dataLength,0);

  // output keys
  if (keyFileBaseName != NULL)
  {
    // create directory if it does not exists
    pathName = File_getFilePathNameCString(String_new(),keyFileBaseName);
    if      (!File_exists(pathName))
    {
      error = File_makeDirectory(pathName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
      if (error != ERROR_NONE)
      {
        printError("Cannot create directory '%s' (error: %s)!\n",String_cString(pathName),Error_getText(error));
        String_delete(pathName);
        Password_done(&cryptPassword);
        String_delete(data);
        String_delete(privateKeyFileName);
        String_delete(publicKeyFileName);
        return error;
      }
    }
    else if (!File_isDirectory(pathName))
    {
      printError("'%s' is not a directory!\n",String_cString(pathName));
      String_delete(pathName);
      Password_done(&cryptPassword);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    String_delete(pathName);

    // write encryption public key
    error = Crypt_writePublicPrivateKeyFile(&publicKey,
                                            publicKeyFileName,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // password
                                            NULL,  // salt
                                            0  // saltLength
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot write encryption public key file (error: %s)!\n",Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      Password_done(&cryptPassword);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created public encryption key '%s'\n",String_cString(publicKeyFileName));

    // write encryption private key
    error = Crypt_writePublicPrivateKeyFile(&privateKey,
                                            privateKeyFileName,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_FUNCTION,
                                            &cryptPassword,
                                            NULL,  // salt
                                            0  // saltLength
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot write encryption private key file (error: %s)!\n",Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      Password_done(&cryptPassword);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created private encryption key '%s'\n",String_cString(privateKeyFileName));
  }
  else
  {
    // output encryption public key
    error = Crypt_getPublicPrivateKeyString(&publicKey,
                                            data,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // password,
                                            NULL,  // salt
                                            0  // saltLength
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot get encryption public key (error: %s)!\n",Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      Password_done(&cryptPassword);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("crypt-public-key = base64:%s\n",String_cString(data));

    // output encryption private key
    error = Crypt_getPublicPrivateKeyString(&privateKey,
                                            data,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // password,
                                            NULL,  // salt
                                            0  // saltLength
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot get encryption private key (error: %s)!\n",Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      Password_done(&cryptPassword);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("crypt-private-key = base64:%s\n",String_cString(data));
  }
  Crypt_doneKey(&privateKey);
  Crypt_doneKey(&publicKey);

  // free resources
  Password_done(&cryptPassword);
  String_delete(data);
  String_delete(privateKeyFileName);
  String_delete(publicKeyFileName);

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
  String   keyString;
  Errors   error;
  CryptKey publicKey,privateKey;
  String   pathName;

  // initialize variables
  publicKeyFileName  = String_new();
  privateKeyFileName = String_new();
  keyString          = String_new();

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
      printError("Public key file '%s' already exists!\n",String_cString(publicKeyFileName));
      String_delete(keyString);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
    if (File_exists(privateKeyFileName))
    {
      printError("Private key file '%s' already exists!\n",String_cString(privateKeyFileName));
      String_delete(keyString);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
  }

  // generate new key pair for signature
  error = Crypt_createPublicPrivateKeyPair(&publicKey,&privateKey,keyBits,CRYPT_PADDING_TYPE_NONE,CRYPT_KEY_MODE_NONE);
  if (error != ERROR_NONE)
  {
    printError("Cannot create signature key pair (error: %s)!\n",Error_getText(error));
    Crypt_doneKey(&privateKey);
    Crypt_doneKey(&publicKey);
    String_delete(keyString);
    String_delete(privateKeyFileName);
    String_delete(publicKeyFileName);
    return error;
  }

  // output keys
  if (keyFileBaseName != NULL)
  {
    // create directory if it does not exists
    pathName = File_getFilePathNameCString(String_new(),keyFileBaseName);
    if      (!File_exists(pathName))
    {
      error = File_makeDirectory(pathName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
      if (error != ERROR_NONE)
      {
        printError("Cannot create directory '%s' (error: %s)!\n",String_cString(pathName),Error_getText(error));
        String_delete(pathName);
        Crypt_doneKey(&privateKey);
        Crypt_doneKey(&publicKey);
        String_delete(keyString);
        String_delete(privateKeyFileName);
        String_delete(publicKeyFileName);
        return error;
      }
    }
    else if (!File_isDirectory(pathName))
    {
      printError("'%s' is not a directory!\n",String_cString(pathName));
      String_delete(pathName);
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(keyString);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    String_delete(pathName);

    // write signature public key
    error = Crypt_writePublicPrivateKeyFile(&publicKey,
                                            publicKeyFileName,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // password
                                            NULL,  // salt
                                            0  // saltLength
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot write signature public key file!\n");
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(keyString);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created public signature key '%s'\n",String_cString(publicKeyFileName));

    // write signature private key
    error = Crypt_writePublicPrivateKeyFile(&privateKey,
                                            privateKeyFileName,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // password,
                                            NULL,  // salt
                                            0  // saltLength
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot write signature private key file!\n");
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(keyString);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created private signature key '%s'\n",String_cString(privateKeyFileName));
  }
  else
  {
    // output signature public key
    error = Crypt_getPublicPrivateKeyString(&publicKey,
                                            keyString,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // password,
                                            NULL,  // salt
                                            0  // saltLength
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot get signature public key!\n");
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(keyString);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("signature-public-key = base64:%s\n",String_cString(keyString));

    // output signature private key
    error = Crypt_getPublicPrivateKeyString(&privateKey,
                                            keyString,
                                            CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // password,
                                            NULL,  // salt
                                            0  // saltLength
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot get signature private key!\n");
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(keyString);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("signature-private-key = base64:%s\n",String_cString(keyString));
  }
  Crypt_doneKey(&privateKey);
  Crypt_doneKey(&publicKey);

  // free resources
  String_delete(keyString);
  String_delete(privateKeyFileName);
  String_delete(publicKeyFileName);

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

  // create pid file
  error = createPIDFile();
  if (error != ERROR_NONE)
  {
    closeLog();
    return error;
  }

  // init continuous
  error = Continuous_init(continuousDatabaseFileName);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialise continuous (error: %s)!\n",
               Error_getText(error)
              );
    deletePIDFile();
    closeLog();
    return error;
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
                     serverPassword,
                     serverMaxConnections,
                     serverJobsDirectory,
                     indexDatabaseFileName,
                     &jobOptions
                    );
  if (error != ERROR_NONE)
  {
    Continuous_done();
    deletePIDFile();
    closeLog();
    return error;
  }

  // done continouous
  Continuous_done();

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
                       STDOUT_FILENO
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
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runJob(void)
{
  Errors error;

  // get include/excluded entries from commands
  if (!stringIsEmpty(includeFileCommand))
  {
    error = addIncludeListCommand(ENTRY_TYPE_FILE,&includeEntryList,includeFileCommand);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  if (!stringIsEmpty(includeImageCommand))
  {
    error = addIncludeListCommand(ENTRY_TYPE_IMAGE,&includeEntryList,includeImageCommand);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  if (!stringIsEmpty(excludeCommand))
  {
    error = addExcludeListCommand(&excludePatternList,excludeCommand);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // start job execution
  globalOptions.runMode = RUN_MODE_INTERACTIVE;

  // create archive
  error = Command_create(NULL, // job UUID
                         NULL, // schedule UUID
NULL, // masterSocketHandle
                         storageName,
                         &includeEntryList,
                         &excludePatternList,
                         &mountList,
                         &compressExcludePatternList,
                         &deltaSourceList,
                         &jobOptions,
                         ARCHIVE_TYPE_NORMAL,
                         NULL, // scheduleTitle
                         NULL, // scheduleCustomText
                         CALLBACK(getPasswordConsole,NULL),
                         CALLBACK(NULL,NULL), // createStatusInfoFunction
                         CALLBACK(NULL,NULL), // storageRequestVolumeFunction
                         NULL, // pauseCreateFlag
                         NULL, // pauseStorageFlag
                         NULL,  // requestedAbortFlag,
                         NULL  // logHandle
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }

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
  Errors error;

  // get include/excluded entries from commands
  if (!stringIsEmpty(includeFileCommand))
  {
    error = addIncludeListCommand(ENTRY_TYPE_FILE,&includeEntryList,includeFileCommand);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  if (!stringIsEmpty(includeImageCommand))
  {
    error = addIncludeListCommand(ENTRY_TYPE_IMAGE,&includeEntryList,includeImageCommand);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  if (!stringIsEmpty(excludeCommand))
  {
    error = addExcludeListCommand(&excludePatternList,excludeCommand);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // interactive mode
  globalOptions.runMode = RUN_MODE_INTERACTIVE;

  error = ERROR_NONE;
  switch (command)
  {
    case COMMAND_CREATE_FILES:
    case COMMAND_CREATE_IMAGES:
      {
        String     storageName;
        EntryTypes entryType;
        int        z;

        // get storage name
        storageName = String_new();
        if (argc > 1)
        {
          String_setCString(storageName,argv[1]);
        }
        else
        {
          printError("No archive file name given!\n");
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
          for (z = 2; z < argc; z++)
          {
            error = EntryList_appendCString(&includeEntryList,entryType,argv[z],jobOptions.patternType,NULL);
            if (error != ERROR_NONE)
            {
              break;
            }
          }
        }

        if (error == ERROR_NONE)
        {
          // create archive
          error = Command_create(NULL, // job UUID
                                 NULL, // schedule UUID
NULL, // masterSocketHandle
                                 storageName,
                                 &includeEntryList,
                                 &excludePatternList,
                                 &mountList,
                                 &compressExcludePatternList,
                                 &deltaSourceList,
                                 &jobOptions,
                                 ARCHIVE_TYPE_NORMAL,
                                 NULL, // scheduleTitle
                                 NULL, // scheduleCustomText
                                 CALLBACK(getPasswordConsole,NULL),
                                 CALLBACK(NULL,NULL), // createStatusInfoFunction
                                 CALLBACK(NULL,NULL), // storageRequestVolumeFunction
                                 NULL, // pauseCreateFlag
                                 NULL, // pauseStorageFlag
                                 NULL,  // requestedAbortFlag,
                                 NULL  // logHandle
                                );
        }

        // free resources
        String_delete(storageName);
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
        int        z;

        // get archive files
        StringList_init(&storageNameList);
        for (z = 1; z < argc; z++)
        {
          StringList_appendCString(&storageNameList,argv[z]);
        }

        switch (command)
        {
          case COMMAND_NONE:
            if      (globalOptions.metaInfoFlag)
            {
              // default: show meta-info only
              error = Command_list(&storageNameList,
                                   &includeEntryList,
                                   &excludePatternList,
                                      globalOptions.longFormatFlag  // showContentFlag
                                   || globalOptions.humanFormatFlag,
                                   &jobOptions,
                                   CALLBACK(getPasswordConsole,NULL),
                                   NULL  // logHandle
                                  );
            }
            else
            {
              // default: list archives content
              error = Command_list(&storageNameList,
                                   &includeEntryList,
                                   &excludePatternList,
                                   TRUE,  // showContentFlag
                                   &jobOptions,
                                   CALLBACK(getPasswordConsole,NULL),
                                   NULL  // logHandle
                                  );
            }
            break;
          case COMMAND_LIST:
            error = Command_list(&storageNameList,
                                 &includeEntryList,
                                 &excludePatternList,
                                 TRUE,  // showContentFlag
                                 &jobOptions,
                                 CALLBACK(getPasswordConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_TEST:
            error = Command_test(&storageNameList,
                                 &includeEntryList,
                                 &excludePatternList,
                                 &deltaSourceList,
                                 &jobOptions,
                                 CALLBACK(getPasswordConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_COMPARE:
            error = Command_compare(&storageNameList,
                                    &includeEntryList,
                                    &excludePatternList,
                                    &deltaSourceList,
                                    &jobOptions,
                                    CALLBACK(getPasswordConsole,NULL),
                                    NULL  // logHandle
                                   );
            break;
          case COMMAND_RESTORE:
            error = Command_restore(&storageNameList,
                                    &includeEntryList,
                                    &excludePatternList,
                                    &deltaSourceList,
                                    &jobOptions,
                                    CALLBACK(NULL,NULL),  // restoreStatusInfo callback
                                    CALLBACK(NULL,NULL),  // restoreError callback
                                    CALLBACK(getPasswordConsole,NULL),
                                    CALLBACK(NULL,NULL),  // isPause callback
                                    CALLBACK(NULL,NULL),  // isAborted callback
                                    NULL  // logHandle
                                   );
            break;
          case COMMAND_CONVERT:
            error = Command_convert(&storageNameList,
                                    &jobOptions,
                                    CALLBACK(getPasswordConsole,NULL),
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
        error = generateEncryptionKeys(keyFileName);
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
      printError("No command given!\n");
      error = ERROR_INVALID_ARGUMENT;
      break;
  }

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
  Errors     error;
  StringNode *stringNode;
  String     fileName;
  bool       printInfoFlag;

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
    return error;
  }
  globalOptions.barExecutable = argv[0];

//TODO remove
#if 0
{
CryptKey p,s;
String n,e;

Crypt_createKeyPair(&p,&s,1024);
Crypt_dumpKey(&p);
//Crypt_dumpKey(&s);
n = Crypt_getKeyModulus(&p);
e = Crypt_getKeyExponent(&p);
fprintf(stderr,"%s, %d: n=%s\n",__FILE__,__LINE__,String_cString(n));
fprintf(stderr,"%s, %d: e=%s\n",__FILE__,__LINE__,String_cString(e));

exit(1);
}
#endif

  // parse command line: pre-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,1,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    doneAll();
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return ERROR_INVALID_ARGUMENT;
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
                       0,2,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    doneAll();
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return ERROR_INVALID_ARGUMENT;
  }

  // if daemon then print info
  printInfoFlag = !globalOptions.quietFlag && daemonFlag;

  // read all configuration files
  STRINGLIST_ITERATE(&configFileNameList,stringNode,fileName)
  {
    if (!readConfigFile(fileName,printInfoFlag))
    {
      doneAll();
      #ifndef NDEBUG
        debugResourceDone();
        File_debugDone();
        Array_debugDone();
        String_debugDone();
        List_debugDone();
      #endif /* not NDEBUG */
      return ERROR_CONFIG;
    }
  }

  // special case: set verbose level in interactive mode
  if (!daemonFlag && !batchFlag)
  {
    globalOptions.verboseLevel = DEFAULT_VERBOSE_LEVEL_INTERACTIVE;
  }

  // read options from job file
  if (jobName != NULL)
  {
    fileName = String_new();

    File_setFileNameCString(fileName,serverJobsDirectory);
    File_appendFileName(fileName,jobName);
    if (!readFromJob(fileName))
    {
      String_delete(fileName);
      return ERROR_CONFIG;
    }

    String_delete(fileName);
  }

  // parse command line: all
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,CMD_PRIORITY_ANY,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    doneAll();
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
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

    doneAll();
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return ERROR_NONE;
  }
  if (helpFlag || xhelpFlag || helpInternalFlag)
  {
    if      (helpInternalFlag) printUsage(argv[0],2);
    else if (xhelpFlag       ) printUsage(argv[0],1);
    else                       printUsage(argv[0],0);

    doneAll();
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return ERROR_NONE;
  }

  // check parameters
  if (!validateOptions())
  {
    doneAll();
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return ERROR_INVALID_ARGUMENT;
  }

  // create temporary directory
  error = File_getTmpDirectoryNameCString(tmpDirectory,"bar-XXXXXX",globalOptions.tmpDirectory);
  if (error != ERROR_NONE)
  {
    printError("Cannot create temporary directory in '%s' (error: %s)!\n",
               String_cString(globalOptions.tmpDirectory),
               Error_getText(error)
              );
    doneAll();
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
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
  else if (jobName != NULL)
  {
    error = runJob();
  }
  else
  {
    error = runInteractive(argc,argv);
  }

  // delete temporary directory
  File_delete(tmpDirectory,TRUE);

  // free resources
  doneAll();
  #ifndef NDEBUG
    debugResourceDone();
    File_debugDone();
    Array_debugDone();
    String_debugDone();
    List_debugDone();
  #endif /* not NDEBUG */

  return error;
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  Errors error;

  assert(argc >= 0);

  // parse command line: pre-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,0,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return errorToExitcode(ERROR_INVALID_ARGUMENT);
  }

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
      if (daemon(1,0) == 0)
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

  return errorToExitcode(error);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
