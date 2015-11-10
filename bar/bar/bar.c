/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver main program
* Systems: all
*
\***********************************************************************/

#define __BAR_IMPLEMENATION__

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
#define DEFAULT_JOBS_DIRECTORY                CONFIG_DIR "/jobs"
#define DEFAULT_CD_DEVICE_NAME                "/dev/cdrw"
#define DEFAULT_DVD_DEVICE_NAME               "/dev/dvd"
#define DEFAULT_BD_DEVICE_NAME                "/dev/bd"
#define DEFAULT_DEVICE_NAME                   "/dev/raw"

#define DEFAULT_CD_VOLUME_SIZE                MAX_INT64
#define DEFAULT_DVD_VOLUME_SIZE               MAX_INT64
#define DEFAULT_BD_VOLUME_SIZE                MAX_INT64
#define DEFAULT_DEVICE_VOLUME_SIZE            MAX_INT64

#define DEFAULT_DATABASE_INDEX_FILE           "/var/lib/bar/index.db"

#define CD_UNLOAD_VOLUME_COMMAND              "eject %device"
#define CD_LOAD_VOLUME_COMMAND                "eject -t %device"
#define CD_IMAGE_COMMAND                      "nice mkisofs -V Backup -volset %number -r -o %image %directory"
#define CD_ECC_COMMAND                        "nice dvdisaster -mRS02 -n cd -c -i %image -v"
#define CD_WRITE_COMMAND                      "nice sh -c 'mkisofs -V Backup -volset %number -r -o %image %directory && cdrecord dev=%device %image'"
#define CD_WRITE_IMAGE_COMMAND                "nice cdrecord dev=%device %image"

#define DVD_UNLOAD_VOLUME_COMMAND             "eject %device"
#define DVD_LOAD_VOLUME_COMMAND               "eject -t %device"
#define DVD_IMAGE_COMMAND                     "nice mkisofs -V Backup -volset %number -r -o %image %directory"
#define DVD_ECC_COMMAND                       "nice dvdisaster -mRS02 -n dvd -c -i %image -v"
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
#define BD_WRITE_COMMAND                      "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %directory"
//#warning todo remove -dry-run
//#define BD_WRITE_COMMAND                      "nice growisofs -Z %device -A BAR -V Backup -volset %number -dry-run -r %directory"
#define BD_WRITE_IMAGE_COMMAND                "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload"
//#define BD_WRITE_IMAGE_COMMAND                "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload  -dry-run"

#define MIN_PASSWORD_QUALITY_LEVEL            0.6

// file name extensions
#define FILE_NAME_EXTENSION_ARCHIVE_FILE      ".bar"
#define FILE_NAME_EXTENSION_INCREMENTAL_FILE  ".bid"

LOCAL const struct
{
  const char   *name;
  ArchiveTypes archiveType;
} ARCHIVE_TYPES[] =
{
  { "normal",       ARCHIVE_TYPE_NORMAL       },
  { "full",         ARCHIVE_TYPE_FULL         },
  { "incremental",  ARCHIVE_TYPE_INCREMENTAL  },
  { "differential", ARCHIVE_TYPE_DIFFERENTIAL }
};

/***************************** Datatypes *******************************/

typedef enum
{
  COMMAND_NONE,

  COMMAND_CREATE_FILES,
  COMMAND_CREATE_IMAGES,
  COMMAND_LIST,
  COMMAND_TEST,
  COMMAND_COMPARE,
  COMMAND_RESTORE,

  COMMAND_GENERATE_KEYS,
  COMMAND_NEW_KEY_PASSWORD,

  COMMAND_UNKNOWN,
} Commands;

/***************************** Variables *******************************/

GlobalOptions         globalOptions;
String                tmpDirectory;
IndexHandle           *indexHandle;
Semaphore             consoleLock;
locale_t              POSIXLocale;

LOCAL Commands        command;
LOCAL String          jobName;

LOCAL JobOptions      jobOptions;
LOCAL String          uuid;
LOCAL String          storageName;
LOCAL ServerList      serverList;
LOCAL Semaphore       serverListLock;
LOCAL DeviceList      deviceList;
LOCAL EntryList       includeEntryList;
LOCAL PatternList     excludePatternList;
LOCAL PatternList     compressExcludePatternList;
LOCAL DeltaSourceList deltaSourceList;
LOCAL Server          defaultFTPServer;
LOCAL Server          defaultSSHServer;
LOCAL Server          defaultWebDAVServer;
LOCAL Device          defaultDevice;
LOCAL Server          *currentFTPServer;
LOCAL Server          *currentSSHServer;
LOCAL Server          *currentWebDAVServer;
LOCAL Device          *currentDevice;
LOCAL bool            daemonFlag;
LOCAL bool            noDetachFlag;
LOCAL uint            serverPort;
LOCAL uint            serverTLSPort;
LOCAL const char      *serverCAFileName;
LOCAL const char      *serverCertFileName;
LOCAL const char      *serverKeyFileName;
LOCAL Password        *serverPassword;
LOCAL const char      *serverJobsDirectory;

LOCAL const char      *indexDatabaseFileName;

LOCAL ulong           logTypes;
LOCAL const char      *logFileName;
LOCAL const char      *logPostCommand;

LOCAL bool            batchFlag;
LOCAL bool            versionFlag;
LOCAL bool            helpFlag,xhelpFlag,helpInternalFlag;

LOCAL const char      *pidFileName;

LOCAL String          keyFileName;
LOCAL uint            keyBits;

LOCAL IndexHandle     __indexHandle;

/*---------------------------------------------------------------------*/

LOCAL StringList         configFileNameList;  // list of configuration files to read

LOCAL String             tmpLogFileName;      // file name of temporary log file
LOCAL Semaphore          logLock;
LOCAL FILE               *logFile = NULL;     // log file handle
LOCAL FILE               *tmpLogFile = NULL;  // temporary log file handle

LOCAL ThreadLocalStorage outputLineHandle;
LOCAL String             lastOutputLine;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL bool cmdOptionParseString(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseConfigFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseEntryPattern(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePattern(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseDeltaSource(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseCompressAlgorithms(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseBandWidth(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseOwner(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionReadKeyFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseOverwriteArchiveFiles(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);

LOCAL const CommandLineUnit COMMAND_LINE_BYTES_UNITS[] = CMD_VALUE_UNIT_ARRAY
(
  {"K",1024LL},
  {"M",1024LL*1024LL},
  {"G",1024LL*1024LL*1024LL},
  {"T",1024LL*1024LL*1024LL*1024LL},
);

LOCAL const CommandLineUnit COMMAND_LINE_BITS_UNITS[] = CMD_VALUE_UNIT_ARRAY
(
  {"K",1024LL},
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
  {"days",24*60*60},
  {"h",60*60},
  {"m",60},
  {"s",1},
);

LOCAL const CommandLineOptionSet COMMAND_LINE_OPTIONS_LOG_TYPES[] = CMD_VALUE_SET_ARRAY
(
  {"none",      LOG_TYPE_NONE,               "no logging"               },
  {"errors",    LOG_TYPE_ERROR,              "log errors"               },
  {"warnings",  LOG_TYPE_WARNING,            "log warnings"             },

  {"ok",        LOG_TYPE_ENTRY_OK,           "log stored/restored files"},
  {"unknown",   LOG_TYPE_ENTRY_TYPE_UNKNOWN, "log unknown files"        },
  {"skipped",   LOG_TYPE_ENTRY_ACCESS_DENIED,"log skipped files"        },
  {"missing",   LOG_TYPE_ENTRY_MISSING,      "log missing files"        },
  {"incomplete",LOG_TYPE_ENTRY_INCOMPLETE,   "log incomplete files"     },
  {"excluded",  LOG_TYPE_ENTRY_EXCLUDED,     "log excluded files"       },

  {"storage",   LOG_TYPE_STORAGE,            "log storage"              },

  {"index",     LOG_TYPE_INDEX,              "index database"           },

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
  CMD_OPTION_ENUM         ("create",                       'c',0,1,command,                                         COMMAND_CREATE_FILES,                                  "create new files archive"                                                 ),
  CMD_OPTION_ENUM         ("image",                        'm',0,1,command,                                         COMMAND_CREATE_IMAGES,                                 "create new images archive"                                                ),
  CMD_OPTION_ENUM         ("list",                         'l',0,1,command,                                         COMMAND_LIST,                                          "list contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("test",                         't',0,1,command,                                         COMMAND_TEST,                                          "test contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("compare",                      'd',0,1,command,                                         COMMAND_COMPARE,                                       "compare contents of archive with files and images"                        ),
  CMD_OPTION_ENUM         ("extract",                      'x',0,1,command,                                         COMMAND_RESTORE,                                       "restore archive"                                                          ),
  CMD_OPTION_ENUM         ("generate-keys",                0,  0,1,command,                                         COMMAND_GENERATE_KEYS,                                 "generate new public/private key pair"                                     ),
//  CMD_OPTION_ENUM         ("new-key-password",             0,  1,1,command,                                         COMMAND_NEW_KEY_PASSWORD,                             "set new private key password"                                             ),
  CMD_OPTION_INTEGER      ("generate-keys-bits",           0,  1,1,keyBits,                                         MIN_ASYMMETRIC_CRYPT_KEY_BITS,
                                                                                                                    MAX_ASYMMETRIC_CRYPT_KEY_BITS,COMMAND_LINE_BITS_UNITS, "key bits",NULL                                                            ),
  CMD_OPTION_STRING       ("job",                          0,  0,1,jobName,                                                                                                "execute job","name"                                                       ),

  CMD_OPTION_ENUM         ("normal",                       0,  1,2,jobOptions.archiveType,                          ARCHIVE_TYPE_NORMAL,                                   "create normal archive (no incremental list file)"                         ),
  CMD_OPTION_ENUM         ("full",                         'f',0,2,jobOptions.archiveType,                          ARCHIVE_TYPE_FULL,                                     "create full archive and incremental list file"                            ),
  CMD_OPTION_ENUM         ("incremental",                  'i',0,2,jobOptions.archiveType,                          ARCHIVE_TYPE_INCREMENTAL,                              "create incremental archive"                                               ),
  CMD_OPTION_SPECIAL      ("incremental-list-file",        'I',1,2,&jobOptions.incrementalListFileName,             cmdOptionParseString,NULL,                             "incremental list file name (default: <archive name>.bid)","file name"     ),
  CMD_OPTION_ENUM         ("differential",                 0,  1,2,jobOptions.archiveType,                          ARCHIVE_TYPE_DIFFERENTIAL,                             "create differential archive"                                              ),

  CMD_OPTION_SELECT       ("pattern-type",                 0,  1,2,jobOptions.patternType,                          COMMAND_LINE_OPTIONS_PATTERN_TYPES,                    "select pattern type"                                                      ),

  CMD_OPTION_SPECIAL      ("include",                      '#',0,3,&includeEntryList,                               cmdOptionParseEntryPattern,NULL,                       "include pattern","pattern"                                                ),
  CMD_OPTION_SPECIAL      ("exclude",                      '!',0,3,&excludePatternList,                             cmdOptionParsePattern,NULL,                            "exclude pattern","pattern"                                                ),

  CMD_OPTION_SPECIAL      ("delta-source",                 0,  0,3,&deltaSourceList,                                cmdOptionParseDeltaSource,NULL,                        "source pattern","pattern"                                                 ),

  CMD_OPTION_SPECIAL      ("config",                       0,  1,1,NULL,                                            cmdOptionParseConfigFile,NULL,                         "configuration file","file name"                                           ),

  CMD_OPTION_STRING       ("tmp-directory",                0,  1,1,globalOptions.tmpDirectory,                                                                             "temporary directory","path"                                               ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",                 0,  1,1,globalOptions.maxTmpSize,                        0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,              "max. size of temporary files","unlimited"                                 ),

  CMD_OPTION_INTEGER64    ("archive-part-size",            's',0,2,jobOptions.archivePartSize,                      0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,              "approximated archive part size","unlimited"                               ),

  CMD_OPTION_INTEGER      ("directory-strip",              'p',1,2,jobOptions.directoryStripCount,                  -1,MAX_INT,NULL,                                       "number of directories to strip on extract",NULL                           ),
  CMD_OPTION_STRING       ("destination",                  0,  0,2,jobOptions.destination,                                                                                 "destination to restore files/images","path"                               ),
  CMD_OPTION_SPECIAL      ("owner",                        0,  0,2,&jobOptions.owner,                               cmdOptionParseOwner,NULL,                              "user and group of restored files","user:group"                            ),

  CMD_OPTION_SPECIAL      ("compress-algorithm",           'z',0,2,&jobOptions.compressAlgorithms,                  cmdOptionParseCompressAlgorithms,NULL,                 "select compress algorithms to use\n"
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
  CMD_OPTION_INTEGER      ("compress-min-size",            0,  1,2,globalOptions.compressMinFileSize,               0,MAX_INT,COMMAND_LINE_BYTES_UNITS,                    "minimal size of file for compression",NULL                                ),
  CMD_OPTION_SPECIAL      ("compress-exclude",             0,  0,3,&compressExcludePatternList,                     cmdOptionParsePattern,NULL,                            "exclude compression pattern","pattern"                                    ),

  CMD_OPTION_SELECT       ("crypt-algorithm",              'y',0,2,jobOptions.cryptAlgorithm,                       COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS,                 "select crypt algorithm to use"                                            ),
  CMD_OPTION_SELECT       ("crypt-type",                   0,  0,2,jobOptions.cryptType,                            COMMAND_LINE_OPTIONS_CRYPT_TYPES,                      "select crypt type"                                                        ),
  CMD_OPTION_SPECIAL      ("crypt-password",               0,  0,2,&globalOptions.cryptPassword,                    cmdOptionParsePassword,NULL,                           "crypt password (use with care!)","password"                               ),
  CMD_OPTION_STRING       ("crypt-public-key",             0,  0,2,jobOptions.cryptPublicKeyFileName,                                                                      "public key for encryption","file name"                                    ),
  CMD_OPTION_STRING       ("crypt-private-key",            0,  0,2,jobOptions.cryptPrivateKeyFileName,                                                                     "private key for decryption","file name"                                   ),

  CMD_OPTION_STRING       ("ftp-login-name",               0,  0,2,defaultFTPServer.ftpServer.loginName,                                                                   "ftp login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ftp-password",                 0,  0,2,&defaultFTPServer.ftpServer.password,            cmdOptionParsePassword,NULL,                           "ftp password (use with care!)","password"                                 ),
  CMD_OPTION_INTEGER      ("ftp-max-connections",          0,  0,2,defaultFTPServer.maxConnectionCount,             0,MAX_INT,NULL,                                        "max. number of concurrent ftp connections","unlimited"                    ),
//TODO
//  CMD_OPTION_INTEGER64    ("ftp-max-storage-size",         0,  0,2,defaultFTPServer.maxStorageSize,                 0LL,MAX_INT64,NULL,                                    "max. number of bytes to store on ftp server","unlimited"                  ),

  CMD_OPTION_INTEGER      ("ssh-port",                     0,  0,2,defaultSSHServer.sshServer.port,                 0,65535,NULL,                                          "ssh port",NULL                                                            ),
  CMD_OPTION_STRING       ("ssh-login-name",               0,  0,2,defaultSSHServer.sshServer.loginName,                                                                   "ssh login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ssh-password",                 0,  0,2,&defaultSSHServer.sshServer.password,            cmdOptionParsePassword,NULL,                           "ssh password (use with care!)","password"                                 ),
  CMD_OPTION_SPECIAL      ("ssh-public-key",               0,  1,2,&defaultSSHServer.sshServer.publicKey,           cmdOptionReadKeyFile,NULL,                             "ssh public key file name","file name"                                     ),
  CMD_OPTION_SPECIAL      ("ssh-private-key",              0,  1,2,&defaultSSHServer.sshServer.privateKey,          cmdOptionReadKeyFile,NULL,                             "ssh private key file name","file name"                                    ),
  CMD_OPTION_INTEGER      ("ssh-max-connections",          0,  0,2,defaultSSHServer.maxConnectionCount,             0,MAX_INT,NULL,                                        "max. number of concurrent ssh connections","unlimited"                    ),
//TODO
//  CMD_OPTION_INTEGER64    ("ssh-max-storage-size",         0,  0,2,defaultSSHServer.maxStorageSize,                 0LL,MAX_INT64,NULL,                                    "max. number of bytes to store on ssh server","unlimited"                  ),

//  CMD_OPTION_INTEGER      ("webdav-port",                  0,  0,2,defaultWebDAVServer.webDAVServer.port,          0,65535,NULL,                                          "WebDAV port",NULL                                                         ),
  CMD_OPTION_STRING       ("webdav-login-name",            0,  0,2,defaultWebDAVServer.webDAVServer.loginName,                                                             "WebDAV login name","name"                                                 ),
  CMD_OPTION_SPECIAL      ("webdav-password",              0,  0,2,&defaultWebDAVServer.webDAVServer.password,      cmdOptionParsePassword,NULL,                           "WebDAV password (use with care!)","password"                              ),
  CMD_OPTION_INTEGER      ("webdav-max-connections",       0,  0,2,defaultWebDAVServer.maxConnectionCount,          0,MAX_INT,NULL,                                        "max. number of concurrent WebDAV connections","unlimited"                 ),
//TODO
//  CMD_OPTION_INTEGER64    ("webdav-max-storage-size",      0,  0,2,defaultWebDAVServer.maxStorageSize,              0LL,MAX_INT64,NULL,                                    "max. number of bytes to store on WebDAV server","unlimited"               ),

  CMD_OPTION_BOOLEAN      ("daemon",                       0,  1,0,daemonFlag,                                                                                             "run in daemon mode"                                                       ),
  CMD_OPTION_BOOLEAN      ("no-detach",                    'D',1,0,noDetachFlag,                                                                                           "do not detach in daemon mode"                                             ),
  CMD_OPTION_INTEGER      ("server-port",                  0,  1,1,serverPort,                                      0,65535,NULL,                                          "server port",NULL                                                         ),
  CMD_OPTION_INTEGER      ("server-tls-port",              0,  1,1,serverTLSPort,                                   0,65535,NULL,                                          "TLS (SSL) server port",NULL                                               ),
  CMD_OPTION_CSTRING      ("server-ca-file",               0,  1,1,serverCAFileName,                                                                                       "TLS (SSL) server certificate authority file (CA file)","file name"        ),
  CMD_OPTION_CSTRING      ("server-cert-file",             0,  1,1,serverCertFileName,                                                                                     "TLS (SSL) server certificate file","file name"                            ),
  CMD_OPTION_CSTRING      ("server-key-file",              0,  1,1,serverKeyFileName,                                                                                      "TLS (SSL) server key file","file name"                                    ),
  CMD_OPTION_SPECIAL      ("server-password",              0,  1,1,&serverPassword,                                 cmdOptionParsePassword,NULL,                           "server password (use with care!)","password"                              ),
  CMD_OPTION_CSTRING      ("server-jobs-directory",        0,  1,1,serverJobsDirectory,                                                                                    "server job directory","path name"                                         ),

  CMD_OPTION_INTEGER      ("nice-level",                   0,  1,1,globalOptions.niceLevel,                         0,19,NULL,                                             "general nice level of processes/threads",NULL                             ),
  CMD_OPTION_INTEGER      ("max-threads",                  0,  1,1,globalOptions.maxThreads,                        0,65535,NULL,                                          "max. number of concurrent compress/encryption threads","cpu cores"        ),

  CMD_OPTION_SPECIAL      ("max-band-width",               0,  1,1,&globalOptions.maxBandWidthList,                 cmdOptionParseBandWidth,NULL,                          "max. network band width to use [bits/s]","number or file name"            ),

  CMD_OPTION_BOOLEAN      ("batch",                        0,  2,1,batchFlag,                                                                                              "run in batch mode"                                                        ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",        0,  1,1,&globalOptions.remoteBARExecutable,              cmdOptionParseString,NULL,                             "remote BAR executable","file name"                                        ),

  CMD_OPTION_STRING       ("mount-device",                 0,  1,2,jobOptions.mountDeviceName,                                                                             "device to mount/unmount","name"                                           ),

  CMD_OPTION_STRING       ("pre-command",                  0,  1,1,jobOptions.preProcessCommand,                                                                           "pre-process command","command"                                            ),
  CMD_OPTION_STRING       ("post-command",                 0,  1,1,jobOptions.postProcessCommand,                                                                          "post-process command","command"                                           ),

  CMD_OPTION_STRING       ("file-write-pre-command",       0,  1,1,globalOptions.file.writePreProcessCommand,                                                              "write file pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("file-write-post-command",      0,  1,1,globalOptions.file.writePostProcessCommand,                                                             "write file post-process command","command"                                ),

  CMD_OPTION_STRING       ("ftp-write-pre-command",        0,  1,1,globalOptions.ftp.writePreProcessCommand,                                                               "write FTP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("ftp-write-post-command",       0,  1,1,globalOptions.ftp.writePostProcessCommand,                                                              "write FTP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("scp-write-pre-command",        0,  1,1,globalOptions.scp.writePreProcessCommand,                                                               "write SCP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("scp-write-post-command",       0,  1,1,globalOptions.scp.writePostProcessCommand,                                                              "write SCP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("sftp-write-pre-command",       0,  1,1,globalOptions.sftp.writePreProcessCommand,                                                              "write SFTP pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("sftp-write-post-command",      0,  1,1,globalOptions.sftp.writePostProcessCommand,                                                             "write SFTP post-process command","command"                                ),

  CMD_OPTION_STRING       ("webdav-write-pre-command",     0,  1,1,globalOptions.webdav.writePreProcessCommand,                                                            "write WebDAV pre-process command","command"                               ),
  CMD_OPTION_STRING       ("webdav-write-post-command",    0,  1,1,globalOptions.webdav.writePostProcessCommand,                                                           "write WebDAV post-process command","command"                              ),

  CMD_OPTION_STRING       ("cd-device",                    0,  1,1,globalOptions.cd.defaultDeviceName,                                                                     "default CD device","device name"                                          ),
  CMD_OPTION_STRING       ("cd-request-volume-command",    0,  1,1,globalOptions.cd.requestVolumeCommand,                                                                  "request new CD volume command","command"                                  ),
  CMD_OPTION_STRING       ("cd-unload-volume-command",     0,  1,1,globalOptions.cd.unloadVolumeCommand,                                                                   "unload CD volume command","command"                                       ),
  CMD_OPTION_STRING       ("cd-load-volume-command",       0,  1,1,globalOptions.cd.loadVolumeCommand,                                                                     "load CD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("cd-volume-size",               0,  1,1,globalOptions.cd.volumeSize,                     0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                "CD volume size","unlimited"                                               ),
  CMD_OPTION_STRING       ("cd-image-pre-command",         0,  1,1,globalOptions.cd.imagePreProcessCommand,                                                                "make CD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("cd-image-post-command",        0,  1,1,globalOptions.cd.imagePostProcessCommand,                                                               "make CD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("cd-image-command",             0,  1,1,globalOptions.cd.imageCommand,                                                                          "make CD image command","command"                                          ),
  CMD_OPTION_STRING       ("cd-ecc-pre-command",           0,  1,1,globalOptions.cd.eccPreProcessCommand,                                                                  "make CD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("cd-ecc-post-command",          0,  1,1,globalOptions.cd.eccPostProcessCommand,                                                                 "make CD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("cd-ecc-command",               0,  1,1,globalOptions.cd.eccCommand,                                                                            "make CD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("cd-write-pre-command",         0,  1,1,globalOptions.cd.writePreProcessCommand,                                                                "write CD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("cd-write-post-command",        0,  1,1,globalOptions.cd.writePostProcessCommand,                                                               "write CD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("cd-write-command",             0,  1,1,globalOptions.cd.writeCommand,                                                                          "write CD command","command"                                               ),
  CMD_OPTION_STRING       ("cd-write-image-command",       0,  1,1,globalOptions.cd.writeImageCommand,                                                                     "write CD image command","command"                                         ),

  CMD_OPTION_STRING       ("dvd-device",                   0,  1,1,globalOptions.dvd.defaultDeviceName,                                                                    "default DVD device","device name"                                         ),
  CMD_OPTION_STRING       ("dvd-request-volume-command",   0,  1,1,globalOptions.dvd.requestVolumeCommand,                                                                 "request new DVD volume command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-unload-volume-command",    0,  1,1,globalOptions.dvd.unloadVolumeCommand,                                                                  "unload DVD volume command","command"                                      ),
  CMD_OPTION_STRING       ("dvd-load-volume-command",      0,  1,1,globalOptions.dvd.loadVolumeCommand,                                                                    "load DVD volume command","command"                                        ),
  CMD_OPTION_INTEGER64    ("dvd-volume-size",              0,  1,1,globalOptions.dvd.volumeSize,                    0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                "DVD volume size","unlimited"                                              ),
  CMD_OPTION_STRING       ("dvd-image-pre-command",        0,  1,1,globalOptions.dvd.imagePreProcessCommand,                                                               "make DVD image pre-process command","command"                             ),
  CMD_OPTION_STRING       ("dvd-image-post-command",       0,  1,1,globalOptions.dvd.imagePostProcessCommand,                                                              "make DVD image post-process command","command"                            ),
  CMD_OPTION_STRING       ("dvd-image-command",            0,  1,1,globalOptions.dvd.imageCommand,                                                                         "make DVD image command","command"                                         ),
  CMD_OPTION_STRING       ("dvd-ecc-pre-command",          0,  1,1,globalOptions.dvd.eccPreProcessCommand,                                                                 "make DVD error-correction codes pre-process command","command"            ),
  CMD_OPTION_STRING       ("dvd-ecc-post-command",         0,  1,1,globalOptions.dvd.eccPostProcessCommand,                                                                "make DVD error-correction codes post-process command","command"           ),
  CMD_OPTION_STRING       ("dvd-ecc-command",              0,  1,1,globalOptions.dvd.eccCommand,                                                                           "make DVD error-correction codes command","command"                        ),
  CMD_OPTION_STRING       ("dvd-write-pre-command",        0,  1,1,globalOptions.dvd.writePreProcessCommand,                                                               "write DVD pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("dvd-write-post-command",       0,  1,1,globalOptions.dvd.writePostProcessCommand,                                                              "write DVD post-process command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-write-command",            0,  1,1,globalOptions.dvd.writeCommand,                                                                         "write DVD command","command"                                              ),
  CMD_OPTION_STRING       ("dvd-write-image-command",      0,  1,1,globalOptions.dvd.writeImageCommand,                                                                    "write DVD image command","command"                                        ),

  CMD_OPTION_STRING       ("bd-device",                    0,  1,1,globalOptions.bd.defaultDeviceName,                                                                     "default BD device","device name"                                          ),
  CMD_OPTION_STRING       ("bd-request-volume-command",    0,  1,1,globalOptions.bd.requestVolumeCommand,                                                                  "request new BD volume command","command"                                  ),
  CMD_OPTION_STRING       ("bd-unload-volume-command",     0,  1,1,globalOptions.bd.unloadVolumeCommand,                                                                   "unload BD volume command","command"                                       ),
  CMD_OPTION_STRING       ("bd-load-volume-command",       0,  1,1,globalOptions.bd.loadVolumeCommand,                                                                     "load BD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("bd-volume-size",               0,  1,1,globalOptions.bd.volumeSize,                     0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                "BD volume size","unlimited"                                               ),
  CMD_OPTION_STRING       ("bd-image-pre-command",         0,  1,1,globalOptions.bd.imagePreProcessCommand,                                                                "make BD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("bd-image-post-command",        0,  1,1,globalOptions.bd.imagePostProcessCommand,                                                               "make BD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("bd-image-command",             0,  1,1,globalOptions.bd.imageCommand,                                                                          "make BD image command","command"                                          ),
  CMD_OPTION_STRING       ("bd-ecc-pre-command",           0,  1,1,globalOptions.bd.eccPreProcessCommand,                                                                  "make BD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("bd-ecc-post-command",          0,  1,1,globalOptions.bd.eccPostProcessCommand,                                                                 "make BD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("bd-ecc-command",               0,  1,1,globalOptions.bd.eccCommand,                                                                            "make BD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("bd-write-pre-command",         0,  1,1,globalOptions.bd.writePreProcessCommand,                                                                "write BD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("bd-write-post-command",        0,  1,1,globalOptions.bd.writePostProcessCommand,                                                               "write BD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("bd-write-command",             0,  1,1,globalOptions.bd.writeCommand,                                                                          "write BD command","command"                                               ),
  CMD_OPTION_STRING       ("bd-write-image-command",       0,  1,1,globalOptions.bd.writeImageCommand,                                                                     "write BD image command","command"                                         ),

  CMD_OPTION_STRING       ("device",                       0,  1,1,defaultDevice.defaultDeviceName,                                                                        "default device","device name"                                             ),
  CMD_OPTION_STRING       ("device-request-volume-command",0,  1,1,defaultDevice.requestVolumeCommand,                                                                     "request new volume command","command"                                     ),
  CMD_OPTION_STRING       ("device-load-volume-command",   0,  1,1,defaultDevice.loadVolumeCommand,                                                                        "load volume command","command"                                            ),
  CMD_OPTION_STRING       ("device-unload-volume-command", 0,  1,1,defaultDevice.unloadVolumeCommand,                                                                      "unload volume command","command"                                          ),
  CMD_OPTION_INTEGER64    ("device-volume-size",           0,  1,1,defaultDevice.volumeSize,                        0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                "volume size","unlimited"                                                  ),
  CMD_OPTION_STRING       ("device-image-pre-command",     0,  1,1,defaultDevice.imagePreProcessCommand,                                                                   "make image pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("device-image-post-command",    0,  1,1,defaultDevice.imagePostProcessCommand,                                                                  "make image post-process command","command"                                ),
  CMD_OPTION_STRING       ("device-image-command",         0,  1,1,defaultDevice.imageCommand,                                                                             "make image command","command"                                             ),
  CMD_OPTION_STRING       ("device-ecc-pre-command",       0,  1,1,defaultDevice.eccPreProcessCommand,                                                                     "make error-correction codes pre-process command","command"                ),
  CMD_OPTION_STRING       ("device-ecc-post-command",      0,  1,1,defaultDevice.eccPostProcessCommand,                                                                    "make error-correction codes post-process command","command"               ),
  CMD_OPTION_STRING       ("device-ecc-command",           0,  1,1,defaultDevice.eccCommand,                                                                               "make error-correction codes command","command"                            ),
  CMD_OPTION_STRING       ("device-write-pre-command",     0,  1,1,defaultDevice.writePreProcessCommand,                                                                   "write device pre-process command","command"                               ),
  CMD_OPTION_STRING       ("device-write-post-command",    0,  1,1,defaultDevice.writePostProcessCommand,                                                                  "write device post-process command","command"                              ),
  CMD_OPTION_STRING       ("device-write-command",         0,  1,1,defaultDevice.writeCommand,                                                                             "write device command","command"                                           ),

  CMD_OPTION_INTEGER64    ("max-storage-size",             0,  1,2,jobOptions.maxStorageSize,                       0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                "max. storage size","unlimited"                                            ),
  CMD_OPTION_INTEGER64    ("volume-size",                  0,  1,2,jobOptions.volumeSize,                           0LL,MAX_INT64,COMMAND_LINE_BYTES_UNITS,                "volume size","unlimited"                                                  ),
  CMD_OPTION_BOOLEAN      ("ecc",                          0,  1,2,jobOptions.errorCorrectionCodesFlag,                                                                    "add error-correction codes with 'dvdisaster' tool"                        ),
  CMD_OPTION_BOOLEAN      ("always-create-image",          0,  1,2,jobOptions.alwaysCreateImageFlag,                                                                       "always create image for CD/DVD/BD/device"                                 ),

  CMD_OPTION_CSTRING      ("index-database",               0,  1,1,indexDatabaseFileName,                                                                                  "index database file name","file name"                                     ),
  CMD_OPTION_BOOLEAN      ("index-database-auto-update",   0,  1,1,globalOptions.indexDatabaseAutoUpdateFlag,                                                              "enabled automatic update index database"                                  ),
  CMD_OPTION_SPECIAL      ("index-database-max-band-width",0,  1,1,&globalOptions.indexDatabaseMaxBandWidthList,    cmdOptionParseBandWidth,NULL,                          "max. band width to use for index updates [bis/s]","number or file name"   ),
  CMD_OPTION_INTEGER      ("index-database-keep-time",     0,  1,1,globalOptions.indexDatabaseKeepTime,             0,MAX_INT,COMMAND_LINE_TIME_UNITS,                     "time to keep index data of not existing storage",NULL                     ),

  CMD_OPTION_SET          ("log",                          0,  1,1,logTypes,                                        COMMAND_LINE_OPTIONS_LOG_TYPES,                        "log types"                                                                ),
  CMD_OPTION_CSTRING      ("log-file",                     0,  1,1,logFileName,                                                                                            "log file name","file name"                                                ),
  CMD_OPTION_CSTRING      ("log-post-command",             0,  1,1,logPostCommand,                                                                                         "log file post-process command","command"                                  ),

  CMD_OPTION_CSTRING      ("pid-file",                     0,  1,1,pidFileName,                                                                                            "process id file name","file name"                                         ),

  CMD_OPTION_BOOLEAN      ("group",                        'g',0,1,globalOptions.groupFlag,                                                                                "group files in list"                                                      ),
  CMD_OPTION_BOOLEAN      ("all",                          0,  0,1,globalOptions.allFlag,                                                                                  "show all files"                                                           ),
  CMD_OPTION_BOOLEAN      ("long-format",                  'L',0,1,globalOptions.longFormatFlag,                                                                           "list in long format"                                                      ),
  CMD_OPTION_BOOLEAN      ("human-format",                 'H',0,1,globalOptions.humanFormatFlag,                                                                          "list in human readable format"                                            ),
  CMD_OPTION_BOOLEAN      ("no-header-footer",             0,  0,1,globalOptions.noHeaderFooterFlag,                                                                       "output no header/footer in list"                                          ),
  CMD_OPTION_BOOLEAN      ("delete-old-archive-files",     0,  1,1,globalOptions.deleteOldArchiveFilesFlag,                                                                "delete old archive files after creating new files"                        ),
  CMD_OPTION_BOOLEAN      ("ignore-no-backup-file",        0,  1,2,globalOptions.ignoreNoBackupFileFlag,                                                                   "ignore .nobackup/.NOBACKUP file"                                          ),
  CMD_OPTION_BOOLEAN      ("ignore-no-dump",               0,  1,2,jobOptions.ignoreNoDumpAttributeFlag,                                                                   "ignore 'no dump' attribute of files"                                      ),

  CMD_OPTION_BOOLEAN      ("skip-unreadable",              0,  0,2,jobOptions.skipUnreadableFlag,                                                                          "skip unreadable files"                                                    ),
  CMD_OPTION_BOOLEAN      ("force-delta-compression",      0,  0,2,jobOptions.forceDeltaCompressionFlag,                                                                   "force delta compression of files. Stop on error"                          ),
  CMD_OPTION_BOOLEAN      ("raw-images",                   0,  1,2,jobOptions.rawImagesFlag,                                                                               "store raw images (store all image blocks)"                                ),
  CMD_OPTION_BOOLEAN      ("no-fragments-check",           0,  1,2,jobOptions.noFragmentsCheckFlag,                                                                        "do not check completeness of file fragments"                              ),
  CMD_OPTION_BOOLEAN      ("no-index-database",            0,  1,1,jobOptions.noIndexDatabaseFlag,                                                                         "do not store index database for archives"                                 ),
  CMD_OPTION_SELECT       ("archive-file-mode",            0,  1,2,jobOptions.archiveFileMode,                      COMMAND_LINE_OPTIONS_ARCHIVE_FILE_MODES,               "select archive files write mode"                                          ),
  CMD_OPTION_BOOLEAN      ("overwrite-archive-files",      'o',0,2,jobOptions.archiveFileModeOverwriteFlag,                                                                "overwrite existing archive files"                                         ),
  CMD_OPTION_BOOLEAN      ("overwrite-files",              0,  0,2,jobOptions.overwriteFilesFlag,                                                                          "overwrite existing files"                                                 ),
  CMD_OPTION_BOOLEAN      ("wait-first-volume",            0,  1,2,jobOptions.waitFirstVolumeFlag,                                                                         "wait for first volume"                                                    ),
  CMD_OPTION_BOOLEAN      ("dry-run",                      0,  1,2,jobOptions.dryRunFlag,                                                                                  "do dry-run (skip storage/restore, incremental data, index database)"      ),
  CMD_OPTION_BOOLEAN      ("no-storage",                   0,  1,2,jobOptions.noStorageFlag,                                                                               "do not store archives (skip storage, index database)"                     ),
  CMD_OPTION_BOOLEAN      ("no-bar-on-medium",             0,  1,2,jobOptions.noBAROnMediumFlag,                                                                           "do not store a copy of BAR on medium"                                     ),
  CMD_OPTION_BOOLEAN      ("stop-on-error",                0,  1,2,jobOptions.stopOnErrorFlag,                                                                             "immediately stop on error"                                                ),

  CMD_OPTION_BOOLEAN      ("no-default-config",            0,  1,1,globalOptions.noDefaultConfigFlag,                                                                      "do not read configuration files " CONFIG_DIR "/bar.cfg and ~/.bar/" DEFAULT_CONFIG_FILE_NAME),
  CMD_OPTION_BOOLEAN      ("quiet",                        0,  1,1,globalOptions.quietFlag,                                                                                "suppress any output"                                                      ),
  CMD_OPTION_INTEGER_RANGE("verbose",                      'v',1,1,globalOptions.verboseLevel,                      0,6,NULL,                                              "verbosity level",NULL                                                     ),

  CMD_OPTION_BOOLEAN      ("server-debug",                 0,  2,1,globalOptions.serverDebugFlag,                                                                          "enable debug mode for server"                                             ),

  CMD_OPTION_BOOLEAN      ("version",                      0  ,0,0,versionFlag,                                                                                            "output version"                                                           ),
  CMD_OPTION_BOOLEAN      ("help",                         'h',0,0,helpFlag,                                                                                               "output this help"                                                         ),
  CMD_OPTION_BOOLEAN      ("xhelp",                        0,  0,0,xhelpFlag,                                                                                              "output help to extended options"                                          ),
  CMD_OPTION_BOOLEAN      ("help-internal",                0,  1,0,helpInternalFlag,                                                                                       "output help to internal options"                                          ),
};

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

const ConfigValueUnit CONFIG_VALUE_BYTES_UNITS[] = CONFIG_VALUE_UNIT_ARRAY
(
  {"K",1024LL},
  {"M",1024LL*1024LL},
  {"G",1024LL*1024LL*1024LL},
  {"T",1024LL*1024LL*1024LL*1024LL},
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
  {"days",24*60*60},
  {"h",60*60},
  {"m",60},
  {"s",1},
);

const ConfigValueSet CONFIG_VALUE_LOG_TYPES[] = CONFIG_VALUE_SET_ARRAY
(
  {"none",      LOG_TYPE_NONE               },
  {"errors",    LOG_TYPE_ERROR              },
  {"warnings",  LOG_TYPE_WARNING            },

  {"ok",        LOG_TYPE_ENTRY_OK           },
  {"unknown",   LOG_TYPE_ENTRY_TYPE_UNKNOWN },
  {"skipped",   LOG_TYPE_ENTRY_ACCESS_DENIED},
  {"missing",   LOG_TYPE_ENTRY_MISSING      },
  {"incomplete",LOG_TYPE_ENTRY_INCOMPLETE   },
  {"excluded",  LOG_TYPE_ENTRY_EXCLUDED     },

  {"storage",   LOG_TYPE_STORAGE            },

  {"index",     LOG_TYPE_INDEX,             },

  {"all",       LOG_TYPE_ALL                },
);

const ConfigValueSelect CONFIG_VALUE_ARCHIVE_FILE_MODES[] = CONFIG_VALUE_SET_ARRAY
(
  {"stop",      ARCHIVE_FILE_MODE_STOP      },
  {"append",    ARCHIVE_FILE_MODE_APPEND    },
  {"overwrite", ARCHIVE_FILE_MODE_OVERWRITE },
);

LOCAL const ConfigValue CONFIG_VALUES[] =
{
  // general settings
  CONFIG_VALUE_SPECIAL  ("config",                       NULL,-1,                                                       configValueParseConfigFile,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_STRING   ("tmp-directory",                &globalOptions.tmpDirectory,-1                                 ),
  CONFIG_VALUE_INTEGER64("max-tmp-size",                 &globalOptions.maxTmpSize,-1,                                  0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER  ("nice-level",                   &globalOptions.niceLevel,-1,                                   0,19,NULL),
  CONFIG_VALUE_INTEGER  ("max-threads",                  &globalOptions.maxThreads,-1,                                  0,65535,NULL),

  CONFIG_VALUE_SPECIAL  ("max-band-width",               &globalOptions.maxBandWidthList,-1,                            configValueParseBandWidth,NULL,NULL,NULL,&globalOptions.maxBandWidthList),

  CONFIG_VALUE_INTEGER  ("compress-min-size",            &globalOptions.compressMinFileSize,-1,                         0,MAX_INT,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL  ("compress-exclude",             &compressExcludePatternList,-1,                                configValueParsePattern,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_CSTRING  ("index-database",               &indexDatabaseFileName,-1                                      ),
  CONFIG_VALUE_BOOLEAN  ("index-database-auto-update",   &globalOptions.indexDatabaseAutoUpdateFlag,-1                  ),
  CONFIG_VALUE_SPECIAL  ("index-database-max-band-width",&globalOptions.indexDatabaseMaxBandWidthList,-1,               configValueParseBandWidth,NULL,NULL,NULL,&globalOptions.indexDatabaseMaxBandWidthList),
  CONFIG_VALUE_INTEGER  ("index-database-keep-time",     &globalOptions.indexDatabaseKeepTime,-1,                       0,MAX_INT,CONFIG_VALUE_TIME_UNITS),

  // global job settings
  CONFIG_VALUE_STRING   ("UUID",                         &uuid,-1                                                       ),
  CONFIG_VALUE_IGNORE   ("host-name"),
  CONFIG_VALUE_IGNORE   ("host-port"),
  CONFIG_VALUE_STRING   ("archive-name",                 &storageName,-1                                                ),
  CONFIG_VALUE_SELECT   ("archive-type",                 &jobOptions.archiveType,-1,                                    CONFIG_VALUE_ARCHIVE_TYPES),

  CONFIG_VALUE_STRING   ("incremental-list-file",        &jobOptions.incrementalListFileName,-1                         ),

  CONFIG_VALUE_INTEGER64("archive-part-size",            &jobOptions.archivePartSize,-1,                                0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER  ("directory-strip",              &jobOptions.directoryStripCount,-1,                            -1,MAX_INT,NULL),
  CONFIG_VALUE_STRING   ("destination",                  &jobOptions.destination,-1                                     ),
  CONFIG_VALUE_SPECIAL  ("owner",                        &jobOptions.owner,-1,                                          configValueParseOwner,NULL,NULL,NULL,&jobOptions),

  CONFIG_VALUE_SELECT   ("pattern-type",                 &jobOptions.patternType,-1,                                    CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_VALUE_SPECIAL  ("compress-algorithm",           &jobOptions.compressAlgorithms,-1,                             configValueParseCompressAlgorithms,NULL,NULL,NULL,&jobOptions),

  CONFIG_VALUE_SELECT   ("crypt-algorithm",              &jobOptions.cryptAlgorithm,-1,                                 CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_VALUE_SELECT   ("crypt-type",                   &jobOptions.cryptType,-1,                                      CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_VALUE_SELECT   ("crypt-password-mode",          &jobOptions.cryptPasswordMode,-1,                              CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_VALUE_SPECIAL  ("crypt-password",               &globalOptions.cryptPassword,-1,                               configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_STRING   ("crypt-public-key",             &jobOptions.cryptPublicKeyFileName,-1                          ),
  CONFIG_VALUE_STRING   ("crypt-private-key",            &jobOptions.cryptPrivateKeyFileName,-1                         ),

  CONFIG_VALUE_STRING   ("ftp-login-name",               &currentFTPServer,offsetof(Server,ftpServer.loginName)         ),
  CONFIG_VALUE_SPECIAL  ("ftp-password",                 &currentFTPServer,offsetof(Server,ftpServer.password),         configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_INTEGER  ("ftp-max-connections",          &currentFTPServer,offsetof(Server,maxConnectionCount),         0,MAX_INT,NULL),
//TODO
//  CONFIG_VALUE_INTEGER64("ftp-max-storage-size",         &currentFTPServer,offsetof(Server,maxStorageSize),          0LL,MAX_INT64,NULL),

  CONFIG_VALUE_INTEGER  ("ssh-port",                     &currentSSHServer,offsetof(Server,sshServer.port),             0,65535,NULL),
  CONFIG_VALUE_STRING   ("ssh-login-name",               &currentSSHServer,offsetof(Server,sshServer.loginName)         ),
  CONFIG_VALUE_SPECIAL  ("ssh-password",                 &currentSSHServer,offsetof(Server,sshServer.password),         configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-public-key",               &currentSSHServer,offsetof(Server,sshServer.publicKey),        configValueReadKeyFile,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-private-key",              &currentSSHServer,offsetof(Server,sshServer.privateKey),       configValueReadKeyFile,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_INTEGER  ("ssh-max-connections",          &currentSSHServer,offsetof(Server,maxConnectionCount),         0,MAX_INT,NULL),
//TODO
//  CONFIG_VALUE_INTEGER64("ssh-max-storage-size",         &currentSSHServer,offsetof(Server,maxStorageSize),          0LL,MAX_INT64,NULL),

//  CONFIG_VALUE_INTEGER  ("webdav-port",                  &currentWebDAVServer,offsetof(Server,port),              0,65535,NULL),
  CONFIG_VALUE_STRING   ("webdav-login-name",            &currentWebDAVServer,offsetof(Server,webDAVServer.loginName)   ),
  CONFIG_VALUE_SPECIAL  ("webdav-password",              &currentWebDAVServer,offsetof(Server,webDAVServer.password),   configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_INTEGER  ("webdav-max-connections",       &currentWebDAVServer,offsetof(Server,maxConnectionCount),      0,MAX_INT,NULL),
//TODO
//  CONFIG_VALUE_INTEGER64("webdav-max-storage-size",      &currentWebDAVServer,offsetof(Server,maxStorageSize),    0LL,MAX_INT64,NULL),

  CONFIG_VALUE_SPECIAL  ("include-file",                 &includeEntryList,-1,                                          configValueParseFileEntry,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("include-image",                &includeEntryList,-1,                                          configValueParseImageEntry,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("exclude",                      &excludePatternList,-1,                                        configValueParsePattern,NULL,NULL,NULL,&jobOptions.patternType),

  CONFIG_VALUE_SPECIAL  ("delta-source",                 &deltaSourceList,-1,                                           configValueParseDeltaSource,NULL,NULL,NULL,&jobOptions.patternType),

  CONFIG_VALUE_INTEGER64("max-storage-size",             &jobOptions.maxStorageSize,-1,                                 0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_INTEGER64("volume-size",                  &jobOptions.volumeSize,-1,                                     0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_BOOLEAN  ("ecc",                          &jobOptions.errorCorrectionCodesFlag,-1                        ),
  CONFIG_VALUE_BOOLEAN  ("always-create-image",          &jobOptions.alwaysCreateImageFlag,-1                           ),

  CONFIG_VALUE_BOOLEAN  ("skip-unreadable",              &jobOptions.skipUnreadableFlag,-1                              ),
  CONFIG_VALUE_BOOLEAN  ("raw-images",                   &jobOptions.rawImagesFlag,-1                                   ),
  CONFIG_VALUE_BOOLEAN  ("no-fragments-check",           &jobOptions.noFragmentsCheckFlag,-1                            ),
  CONFIG_VALUE_SELECT   ("archive-file-mode",            &jobOptions.archiveFileMode,-1,                                CONFIG_VALUE_ARCHIVE_FILE_MODES),
  CONFIG_VALUE_BOOLEAN  ("overwrite-files",              &jobOptions.overwriteFilesFlag,-1                              ),
  CONFIG_VALUE_BOOLEAN  ("wait-first-volume",            &jobOptions.waitFirstVolumeFlag,-1                             ),
  CONFIG_VALUE_BOOLEAN  ("no-bar-on-medium",             &jobOptions.noBAROnMediumFlag,-1                               ),
  CONFIG_VALUE_BOOLEAN  ("quiet",                        &globalOptions.quietFlag,-1                                    ),
  CONFIG_VALUE_INTEGER  ("verbose",                      &globalOptions.verboseLevel,-1,                                0,6,NULL),

  // ignored schedule settings (server only)
  CONFIG_VALUE_BEGIN_SECTION("schedule",-1),
  CONFIG_VALUE_IGNORE   ("UUID"),
  CONFIG_VALUE_IGNORE   ("parentUUID"),
  CONFIG_VALUE_IGNORE   ("date"),
  CONFIG_VALUE_IGNORE   ("weekdays"),
  CONFIG_VALUE_IGNORE   ("time"),
  CONFIG_VALUE_IGNORE   ("archive-type"),
  CONFIG_VALUE_IGNORE   ("interval"),
  CONFIG_VALUE_IGNORE   ("text"),
  CONFIG_VALUE_IGNORE   ("min-keep"),
  CONFIG_VALUE_IGNORE   ("max-keep"),
  CONFIG_VALUE_IGNORE   ("max-age"),
  CONFIG_VALUE_IGNORE   ("enabled"),
  CONFIG_VALUE_END_SECTION(),

  // commands
  CONFIG_VALUE_STRING   ("mount-device",                 &jobOptions.mountDeviceName,-1                                 ),

  CONFIG_VALUE_STRING   ("pre-command",                  &jobOptions.preProcessCommand,-1                               ),
  CONFIG_VALUE_STRING   ("post-command",                 &jobOptions.postProcessCommand,-1                              ),

  CONFIG_VALUE_STRING   ("file-write-pre-command",       &globalOptions.file.writePreProcessCommand,-1                  ),
  CONFIG_VALUE_STRING   ("file-write-post-command",      &globalOptions.file.writePostProcessCommand,-1                 ),

  CONFIG_VALUE_STRING   ("ftp-write-pre-command",        &globalOptions.ftp.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING   ("ftp-write-post-command",       &globalOptions.ftp.writePostProcessCommand,-1                  ),

  CONFIG_VALUE_STRING   ("scp-write-pre-command",        &globalOptions.scp.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING   ("scp-write-post-command",       &globalOptions.scp.writePostProcessCommand,-1                  ),

  CONFIG_VALUE_STRING   ("sftp-write-pre-command",       &globalOptions.sftp.writePreProcessCommand,-1                  ),
  CONFIG_VALUE_STRING   ("sftp-write-post-command",      &globalOptions.sftp.writePostProcessCommand,-1                 ),

  CONFIG_VALUE_STRING   ("webdav-write-pre-command",     &globalOptions.webdav.writePreProcessCommand,-1                ),
  CONFIG_VALUE_STRING   ("webdav-write-post-command",    &globalOptions.webdav.writePostProcessCommand,-1               ),

  CONFIG_VALUE_STRING   ("cd-device",                    &globalOptions.bd.defaultDeviceName,-1                         ),
  CONFIG_VALUE_STRING   ("cd-request-volume-command",    &globalOptions.cd.requestVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING   ("cd-unload-volume-command",     &globalOptions.cd.unloadVolumeCommand,-1                       ),
  CONFIG_VALUE_STRING   ("cd-load-volume-command",       &globalOptions.cd.loadVolumeCommand,-1                         ),
  CONFIG_VALUE_INTEGER64("cd-volume-size",               &globalOptions.cd.volumeSize,-1,                               0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("cd-image-pre-command",         &globalOptions.cd.imagePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING   ("cd-image-post-command",        &globalOptions.cd.imagePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING   ("cd-image-command",             &globalOptions.cd.imageCommand,-1                              ),
  CONFIG_VALUE_STRING   ("cd-ecc-pre-command",           &globalOptions.cd.eccPreProcessCommand,-1                      ),
  CONFIG_VALUE_STRING   ("cd-ecc-post-command",          &globalOptions.cd.eccPostProcessCommand,-1                     ),
  CONFIG_VALUE_STRING   ("cd-ecc-command",               &globalOptions.cd.eccCommand,-1                                ),
  CONFIG_VALUE_STRING   ("cd-write-pre-command",         &globalOptions.cd.writePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING   ("cd-write-post-command",        &globalOptions.cd.writePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING   ("cd-write-command",             &globalOptions.cd.writeCommand,-1                              ),
  CONFIG_VALUE_STRING   ("cd-write-image-command",       &globalOptions.cd.writeImageCommand,-1                         ),

  CONFIG_VALUE_STRING   ("dvd-device",                   &globalOptions.bd.defaultDeviceName,-1                         ),
  CONFIG_VALUE_STRING   ("dvd-request-volume-command",   &globalOptions.dvd.requestVolumeCommand,-1                     ),
  CONFIG_VALUE_STRING   ("dvd-unload-volume-command",    &globalOptions.dvd.unloadVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING   ("dvd-load-volume-command",      &globalOptions.dvd.loadVolumeCommand,-1                        ),
  CONFIG_VALUE_INTEGER64("dvd-volume-size",              &globalOptions.dvd.volumeSize,-1,                              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("dvd-image-pre-command",        &globalOptions.dvd.imagePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING   ("dvd-image-post-command",       &globalOptions.dvd.imagePostProcessCommand,-1                  ),
  CONFIG_VALUE_STRING   ("dvd-image-command",            &globalOptions.dvd.imageCommand,-1                             ),
  CONFIG_VALUE_STRING   ("dvd-ecc-pre-command",          &globalOptions.dvd.eccPreProcessCommand,-1                     ),
  CONFIG_VALUE_STRING   ("dvd-ecc-post-command",         &globalOptions.dvd.eccPostProcessCommand,-1                    ),
  CONFIG_VALUE_STRING   ("dvd-ecc-command",              &globalOptions.dvd.eccCommand,-1                               ),
  CONFIG_VALUE_STRING   ("dvd-write-pre-command",        &globalOptions.dvd.writePreProcessCommand,-1                   ),
  CONFIG_VALUE_STRING   ("dvd-write-post-command",       &globalOptions.dvd.writePostProcessCommand,-1                  ),
  CONFIG_VALUE_STRING   ("dvd-write-command",            &globalOptions.dvd.writeCommand,-1                             ),
  CONFIG_VALUE_STRING   ("dvd-write-image-command",      &globalOptions.dvd.writeImageCommand,-1                        ),

  CONFIG_VALUE_STRING   ("bd-device",                    &globalOptions.bd.defaultDeviceName,-1                         ),
  CONFIG_VALUE_STRING   ("bd-request-volume-command",    &globalOptions.bd.requestVolumeCommand,-1                      ),
  CONFIG_VALUE_STRING   ("bd-unload-volume-command",     &globalOptions.bd.unloadVolumeCommand,-1                       ),
  CONFIG_VALUE_STRING   ("bd-load-volume-command",       &globalOptions.bd.loadVolumeCommand,-1                         ),
  CONFIG_VALUE_INTEGER64("bd-volume-size",               &globalOptions.bd.volumeSize,-1,                               0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("bd-image-pre-command",         &globalOptions.bd.imagePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING   ("bd-image-post-command",        &globalOptions.bd.imagePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING   ("bd-image-command",             &globalOptions.bd.imageCommand,-1                              ),
  CONFIG_VALUE_STRING   ("bd-ecc-pre-command",           &globalOptions.bd.eccPreProcessCommand,-1                      ),
  CONFIG_VALUE_STRING   ("bd-ecc-post-command",          &globalOptions.bd.eccPostProcessCommand,-1                     ),
  CONFIG_VALUE_STRING   ("bd-ecc-command",               &globalOptions.bd.eccCommand,-1                                ),
  CONFIG_VALUE_STRING   ("bd-write-pre-command",         &globalOptions.bd.writePreProcessCommand,-1                    ),
  CONFIG_VALUE_STRING   ("bd-write-post-command",        &globalOptions.bd.writePostProcessCommand,-1                   ),
  CONFIG_VALUE_STRING   ("bd-write-command",             &globalOptions.bd.writeCommand,-1                              ),
  CONFIG_VALUE_STRING   ("bd-write-image-command",       &globalOptions.bd.writeImageCommand,-1                         ),

  CONFIG_VALUE_STRING   ("device",                       &currentDevice,offsetof(Device,defaultDeviceName)              ),
  CONFIG_VALUE_STRING   ("device-request-volume-command",&currentDevice,offsetof(Device,requestVolumeCommand)           ),
  CONFIG_VALUE_STRING   ("device-unload-volume-command", &currentDevice,offsetof(Device,unloadVolumeCommand)            ),
  CONFIG_VALUE_STRING   ("device-load-volume-command",   &currentDevice,offsetof(Device,loadVolumeCommand)              ),
  CONFIG_VALUE_INTEGER64("device-volume-size",           &currentDevice,offsetof(Device,volumeSize),                    0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("device-image-pre-command",     &currentDevice,offsetof(Device,imagePreProcessCommand)         ),
  CONFIG_VALUE_STRING   ("device-image-post-command",    &currentDevice,offsetof(Device,imagePostProcessCommand)        ),
  CONFIG_VALUE_STRING   ("device-image-command",         &currentDevice,offsetof(Device,imageCommand)                   ),
  CONFIG_VALUE_STRING   ("device-ecc-pre-command",       &currentDevice,offsetof(Device,eccPreProcessCommand)           ),
  CONFIG_VALUE_STRING   ("device-ecc-post-command",      &currentDevice,offsetof(Device,eccPostProcessCommand)          ),
  CONFIG_VALUE_STRING   ("device-ecc-command",           &currentDevice,offsetof(Device,eccCommand)                     ),
  CONFIG_VALUE_STRING   ("device-write-pre-command",     &currentDevice,offsetof(Device,writePreProcessCommand)         ),
  CONFIG_VALUE_STRING   ("device-write-post-command",    &currentDevice,offsetof(Device,writePostProcessCommand)        ),
  CONFIG_VALUE_STRING   ("device-write-command",         &currentDevice,offsetof(Device,writeCommand)                   ),

  // server settings
  CONFIG_VALUE_INTEGER  ("server-port",                  &serverPort,-1,                                                0,65535,NULL),
  CONFIG_VALUE_INTEGER  ("server-tls-port",              &serverTLSPort,-1,                                             0,65535,NULL),
  CONFIG_VALUE_CSTRING  ("server-ca-file",               &serverCAFileName,-1                                           ),
  CONFIG_VALUE_CSTRING  ("server-cert-file",             &serverCertFileName,-1                                         ),
  CONFIG_VALUE_CSTRING  ("server-key-file",              &serverKeyFileName,-1                                          ),
  CONFIG_VALUE_SPECIAL  ("server-password",              &serverPassword,-1,                                            configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_CSTRING  ("server-jobs-directory",        &serverJobsDirectory,-1                                        ),

  CONFIG_VALUE_STRING   ("remote-bar-executable",        &globalOptions.remoteBARExecutable,-1                          ),

  CONFIG_VALUE_SET      ("log",                          &logTypes,-1,                                                  CONFIG_VALUE_LOG_TYPES),
  CONFIG_VALUE_CSTRING  ("log-file",                     &logFileName,-1                                                ),
  CONFIG_VALUE_CSTRING  ("log-post-command",             &logPostCommand,-1                                             ),

  CONFIG_VALUE_CSTRING  ("pid-file",                     &pidFileName,-1                                                ),

  // deprecated
  CONFIG_VALUE_IGNORE   ("schedule"),
  CONFIG_VALUE_IGNORE   ("overwrite-archive-files"),
};

/*---------------------------------------------------------------------*/

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
  ulong  z;
  char   ch;

  assert(file != NULL);
  assert(Semaphore_isLocked(&consoleLock));

//fprintf(stderr,"%s, %d: string=%s\n",__FILE__,__LINE__,String_cString(string));
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

        // restore line
        (void)fwrite(String_cString(outputLine),1,String_length(outputLine),file);
      }

      // output new string
      (void)fwrite(String_cString(string),1,String_length(string),file);

      // store output string
      STRING_CHAR_ITERATE(string,z,ch)
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
* Name   : initServer
* Purpose: init server
* Input  : server     - server variable
*          name       - server name
*          serverType - server type
* Output : server - initialized server structure
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initServer(Server *server, ConstString name, ServerTypes serverType)
{
  assert(server != NULL);

  if (!Semaphore_init(&server->lock))
  {
    HALT_FATAL_ERROR("cannot initialize server lock semaphore");
  }
  server->name                                = (name != NULL) ? String_duplicate(name) : String_new();
  server->type                                = serverType;
  switch (serverType)
  {
    case SERVER_TYPE_FTP:
      server->ftpServer.loginName             = NULL;
      server->ftpServer.password              = NULL;
      break;
    case SERVER_TYPE_SSH:
      server->sshServer.port                  = 22;
      server->sshServer.loginName             = NULL;
      server->sshServer.password              = NULL;
      initKey(&server->sshServer.publicKey);
      initKey(&server->sshServer.privateKey);
      break;
    case SERVER_TYPE_WEBDAV:
      server->webDAVServer.loginName          = NULL;
      server->webDAVServer.password           = NULL;
      initKey(&server->webDAVServer.publicKey);
      initKey(&server->webDAVServer.privateKey);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  server->maxConnectionCount                  = MAX_CONNECTION_COUNT_UNLIMITED;
  server->maxStorageSize                      = MAX_STORAGE_SIZE_UNLIMITED;
  server->connection.lowPriorityRequestCount  = 0;
  server->connection.highPriorityRequestCount = 0;
  server->connection.count                    = 0;
}

/***********************************************************************\
* Name   : doneServer
* Purpose: done server
* Input  : server - server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneServer(Server *server)
{
  assert(server != NULL);

  switch (server->type)
  {
    case SERVER_TYPE_FTP:
      if (server->ftpServer.password != NULL) Password_delete(server->ftpServer.password);
      if (server->ftpServer.loginName != NULL) String_delete(server->ftpServer.loginName);
      break;
    case SERVER_TYPE_SSH:
      if (isKeyAvailable(&server->sshServer.privateKey)) doneKey(&server->sshServer.privateKey);
      if (isKeyAvailable(&server->sshServer.publicKey)) doneKey(&server->sshServer.publicKey);
      if (server->sshServer.password != NULL) Password_delete(server->sshServer.password);
      if (server->sshServer.loginName != NULL) String_delete(server->sshServer.loginName);
      break;
    case SERVER_TYPE_WEBDAV:
      if (isKeyAvailable(&server->webDAVServer.privateKey)) doneKey(&server->webDAVServer.privateKey);
      if (isKeyAvailable(&server->webDAVServer.publicKey)) doneKey(&server->webDAVServer.publicKey);
      if (server->webDAVServer.password != NULL) Password_delete(server->webDAVServer.password);
      if (server->webDAVServer.loginName != NULL) String_delete(server->webDAVServer.loginName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  String_delete(server->name);
  Semaphore_done(&server->lock);
}

/***********************************************************************\
* Name   : newServerNode
* Purpose: new server node
* Input  : serverType - server type
*          name       - server name
* Output : -
* Return : server node
* Notes  : -
\***********************************************************************/

LOCAL ServerNode *newServerNode(ConstString name, ServerTypes serverType)
{
  ServerNode *serverNode;

  assert(name != NULL);

  serverNode = LIST_NEW_NODE(ServerNode);
  if (serverNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  initServer(&serverNode->server,name,serverType);

  return serverNode;
}

/***********************************************************************\
* Name   : freeServerNode
* Purpose: free server node
* Input  : serverNode - server node
*          userData   - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeServerNode(ServerNode *serverNode, void *userData)
{
  assert(serverNode != NULL);

  UNUSED_VARIABLE(userData);

  doneServer(&serverNode->server);
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

  deviceNode = LIST_NEW_NODE(DeviceNode);
  if (deviceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  initDevice(&deviceNode->device);
  deviceNode->name = String_duplicate(name);

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
  String_delete(deviceNode->device.eccCommand             );
  String_delete(deviceNode->device.eccPostProcessCommand  );
  String_delete(deviceNode->device.eccPreProcessCommand   );
  String_delete(deviceNode->device.imageCommand           );
  String_delete(deviceNode->device.imagePostProcessCommand);
  String_delete(deviceNode->device.imagePreProcessCommand );
  String_delete(deviceNode->device.loadVolumeCommand      );
  String_delete(deviceNode->device.unloadVolumeCommand    );
  String_delete(deviceNode->device.requestVolumeCommand   );
  String_delete(deviceNode->device.defaultDeviceName      );
  String_delete(deviceNode->name);
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
  if (isPrintInfo(2) || printInfoFlag) { printf("Reading configuration file '%s'...",String_cString(fileName)); fflush(stdout); }
  failFlag   = FALSE;
  line       = String_new();
  lineNb     = 0;
  name       = String_new();
  value      = String_new();
  while (File_getLine(&fileHandle,line,&lineNb,"#") && !failFlag)
  {
    // parse line
    if      (String_parse(line,STRING_BEGIN,"[ftp-server %S]",NULL,name))
    {
      ServerNode *serverNode;

      serverNode = newServerNode(name,SERVER_TYPE_FTP);
      List_append(&serverList,serverNode);
      currentFTPServer = &serverNode->server;
    }
    else if (String_parse(line,STRING_BEGIN,"[ssh-server %S]",NULL,name))
    {
      ServerNode *serverNode;

      serverNode = newServerNode(name,SERVER_TYPE_SSH);
      List_append(&serverList,serverNode);
      currentSSHServer = &serverNode->server;
    }
    else if (String_parse(line,STRING_BEGIN,"[webdav-server %S]",NULL,name))
    {
      ServerNode *serverNode;

      serverNode = newServerNode(name,SERVER_TYPE_WEBDAV);
      List_append(&serverList,serverNode);
      currentWebDAVServer = &serverNode->server;
    }
    else if (String_parse(line,STRING_BEGIN,"[device %S]",NULL,name))
    {
      DeviceNode *deviceNode;

      deviceNode = newDeviceNode(name);
      List_append(&deviceList,deviceNode);
      currentDevice = &deviceNode->device;
    }
    else if (String_parse(line,STRING_BEGIN,"[global]",NULL))
    {
      currentFTPServer    = &defaultFTPServer;
      currentSSHServer    = &defaultSSHServer;
      currentWebDAVServer = &defaultWebDAVServer;
      currentDevice       = &defaultDevice;
    }
    else if (String_parse(line,STRING_BEGIN,"[end]",NULL))
    {
      currentFTPServer    = &defaultFTPServer;
      currentSSHServer    = &defaultSSHServer;
      currentWebDAVServer = &defaultWebDAVServer;
      currentDevice       = &defaultDevice;
    }
    else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
      String_unquote(value,STRING_QUOTES);
      if (!ConfigValue_parse(String_cString(name),
                             String_cString(value),
                             CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES),
                             NULL, // section name
                             NULL, // errorOutputHandle,
                             NULL, // errorPrefix,
                             NULL  // variable
                            )
         )
      {
        if (isPrintInfo(2) || printInfoFlag) printf("FAIL!\n");
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
      if (isPrintInfo(2) || printInfoFlag) printf("FAIL!\n");
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
    if (isPrintInfo(2) || printInfoFlag) printf("ok\n");
  }
  currentFTPServer    = &defaultFTPServer;
  currentSSHServer    = &defaultSSHServer;
  currentWebDAVServer = &defaultWebDAVServer;
  currentDevice       = &defaultDevice;

  // free resources
  String_delete(value);
  String_delete(name);
  String_delete(line);

  // close file
  File_close(&fileHandle);

  return !failFlag;
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
    case COMMAND_GENERATE_KEYS:
    case COMMAND_NEW_KEY_PASSWORD:
      entryType = ENTRY_TYPE_FILE;
      break;
    case COMMAND_CREATE_IMAGES:
      entryType = ENTRY_TYPE_IMAGE;
      break;
    default:
      HALT_INTERNAL_ERROR("no valid command set");
      break;
  }

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // append to list
  error = EntryList_appendCString((EntryList*)variable,entryType,value,patternType);
  if (error != ERROR_NONE)
  {
    strncpy(errorMessage,Error_getText(error),errorMessageSize); errorMessage[errorMessageSize-1] = '\0';
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
  error = PatternList_appendCString((PatternList*)variable,value,patternType);
  if (error != ERROR_NONE)
  {
    strncpy(errorMessage,Error_getText(error),errorMessageSize); errorMessage[errorMessageSize-1] = '\0';
    return FALSE;
  }

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
  DeltaSourceList_append((DeltaSourceList*)variable,storageName,patternType);

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
      snprintf(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",algorithm1);
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
      snprintf(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",algorithm2);
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
      snprintf(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",value);
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

  // allocate new schedule node
  bandWidthNode = newBandWidthNode();

  // parse schedule. Format: (<band width>[K|M])|<file name> <date> [<weekday>] <time>
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
  if (nextIndex != STRING_END)
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
    if (nextIndex != STRING_END)
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

  if (errorFlag || (nextIndex != STRING_END))
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
    snprintf(errorMessage,errorMessageSize,"Cannot parse bandwidth value '%s'",value);
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
    snprintf(errorMessage,errorMessageSize,"Unknown owner ship value '%s'",value);
    return FALSE;
  }

  // store owner values
  ((JobOptionsOwner*)variable)->userId  = userId;
  ((JobOptionsOwner*)variable)->groupId = groupId;

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

  if ((*(Password**)variable) == NULL)
  {
    (*(Password**)variable) = Password_new();
  }
  Password_setCString(*(Password**)variable,value);

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
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  error = readKeyFile(key,value);
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseOverwriteArchiveFiles
* Purpose: command line option call back for archive files overwrite mode
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseOverwriteArchiveFiles(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(ArchiveFileModes*)variable) = ARCHIVE_FILE_MODE_OVERWRITE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseConfigFile
* Purpose: command line option call back for parsing configuration file
* Input  : -
* Output : -
* Return : -
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
  globalOptions.compressMinFileSize                             = DEFAULT_COMPRESS_MIN_FILE_SIZE;
  globalOptions.cryptPassword                                   = NULL;
  globalOptions.ftpServer                                       = &defaultFTPServer;
  globalOptions.defaultFTPServer                                = &defaultFTPServer;
  globalOptions.sshServer                                       = &defaultSSHServer;
  globalOptions.defaultSSHServer                                = &defaultSSHServer;
  globalOptions.webDAVServer                                    = &defaultWebDAVServer;
  globalOptions.defaultWebDAVServer                             = &defaultWebDAVServer;

  globalOptions.serverList                                      = &serverList;

  globalOptions.remoteBARExecutable                             = NULL;

  globalOptions.file.writePreProcessCommand                     = NULL;
  globalOptions.file.writePostProcessCommand                    = NULL;

  globalOptions.ftp.writePreProcessCommand                      = NULL;
  globalOptions.ftp.writePostProcessCommand                     = NULL;

  globalOptions.scp.writePreProcessCommand                      = NULL;
  globalOptions.scp.writePostProcessCommand                     = NULL;

  globalOptions.sftp.writePreProcessCommand                     = NULL;
  globalOptions.sftp.writePostProcessCommand                    = NULL;

  globalOptions.cd.defaultDeviceName                            = String_newCString(DEFAULT_CD_DEVICE_NAME);
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
  globalOptions.cd.writePreProcessCommand                       = NULL;
  globalOptions.cd.writePostProcessCommand                      = NULL;
  globalOptions.cd.writeCommand                                 = String_newCString(CD_WRITE_COMMAND);
  globalOptions.cd.writeImageCommand                            = String_newCString(CD_WRITE_IMAGE_COMMAND);

  globalOptions.dvd.defaultDeviceName                           = String_newCString(DEFAULT_DVD_DEVICE_NAME);
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
  globalOptions.dvd.writePreProcessCommand                      = NULL;
  globalOptions.dvd.writePostProcessCommand                     = NULL;
  globalOptions.dvd.writeCommand                                = String_newCString(DVD_WRITE_COMMAND);
  globalOptions.dvd.writeImageCommand                           = String_newCString(DVD_WRITE_IMAGE_COMMAND);

  globalOptions.bd.defaultDeviceName                            = String_newCString(DEFAULT_BD_DEVICE_NAME);
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
  globalOptions.bd.writePreProcessCommand                       = NULL;
  globalOptions.bd.writePostProcessCommand                      = NULL;
  globalOptions.bd.writeCommand                                 = String_newCString(BD_WRITE_COMMAND);
  globalOptions.bd.writeImageCommand                            = String_newCString(BD_WRITE_IMAGE_COMMAND);

  globalOptions.device                                          = globalOptions.defaultDevice;
  globalOptions.deviceList                                      = &deviceList;
  globalOptions.defaultDevice                                   = &defaultDevice;

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
  globalOptions.verboseLevel                                    = 1;

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
  List_done(&globalOptions.indexDatabaseMaxBandWidthList,(ListNodeFreeFunction)freeBandWidthNode,NULL);
  String_delete(globalOptions.bd.writeImageCommand);
  String_delete(globalOptions.bd.writeCommand);
  String_delete(globalOptions.bd.writePostProcessCommand);
  String_delete(globalOptions.bd.writePreProcessCommand);
  String_delete(globalOptions.bd.eccCommand);
  String_delete(globalOptions.bd.eccPostProcessCommand);
  String_delete(globalOptions.bd.eccPreProcessCommand);
  String_delete(globalOptions.bd.imageCommand);
  String_delete(globalOptions.bd.imagePostProcessCommand);
  String_delete(globalOptions.bd.imagePreProcessCommand);
  String_delete(globalOptions.bd.loadVolumeCommand);
  String_delete(globalOptions.bd.unloadVolumeCommand);
  String_delete(globalOptions.bd.requestVolumeCommand);
  String_delete(globalOptions.bd.defaultDeviceName);

  String_delete(globalOptions.dvd.writeImageCommand);
  String_delete(globalOptions.dvd.writeCommand);
  String_delete(globalOptions.dvd.writePostProcessCommand);
  String_delete(globalOptions.dvd.writePreProcessCommand);
  String_delete(globalOptions.dvd.eccCommand);
  String_delete(globalOptions.dvd.eccPostProcessCommand);
  String_delete(globalOptions.dvd.eccPreProcessCommand);
  String_delete(globalOptions.dvd.imageCommand);
  String_delete(globalOptions.dvd.imagePostProcessCommand);
  String_delete(globalOptions.dvd.imagePreProcessCommand);
  String_delete(globalOptions.dvd.loadVolumeCommand);
  String_delete(globalOptions.dvd.unloadVolumeCommand);
  String_delete(globalOptions.dvd.requestVolumeCommand);
  String_delete(globalOptions.dvd.defaultDeviceName);

  String_delete(globalOptions.cd.writeImageCommand);
  String_delete(globalOptions.cd.writeCommand);
  String_delete(globalOptions.cd.writePostProcessCommand);
  String_delete(globalOptions.cd.writePreProcessCommand);
  String_delete(globalOptions.cd.eccCommand);
  String_delete(globalOptions.cd.eccPostProcessCommand);
  String_delete(globalOptions.cd.eccPreProcessCommand);
  String_delete(globalOptions.cd.imageCommand);
  String_delete(globalOptions.cd.imagePostProcessCommand);
  String_delete(globalOptions.cd.imagePreProcessCommand);
  String_delete(globalOptions.cd.loadVolumeCommand);
  String_delete(globalOptions.cd.unloadVolumeCommand);
  String_delete(globalOptions.cd.requestVolumeCommand);
  String_delete(globalOptions.cd.defaultDeviceName);

  String_delete(globalOptions.sftp.writePostProcessCommand);
  String_delete(globalOptions.sftp.writePreProcessCommand);

  String_delete(globalOptions.scp.writePostProcessCommand);
  String_delete(globalOptions.scp.writePreProcessCommand);

  String_delete(globalOptions.ftp.writePostProcessCommand);
  String_delete(globalOptions.ftp.writePreProcessCommand);

  String_delete(globalOptions.file.writePostProcessCommand);
  String_delete(globalOptions.file.writePreProcessCommand);

  String_delete(globalOptions.remoteBARExecutable);
  List_done(&globalOptions.maxBandWidthList,(ListNodeFreeFunction)freeBandWidthNode,NULL);
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
  AutoFreeList autoFreeList;
  Errors       error;
  const char   *localePath;
  String       fileName;

  // initialize crash dump handler
  #if HAVE_BREAKPAD
    if (!MiniDump_init())
    {
      (void)fprintf(stderr,"Warning: Cannot initialize crash dump handler. No crash dumps will be created.\n");
    }
  #endif /* HAVE_BREAKPAD */

  // initialize variables
  AutoFree_init(&autoFreeList);

  Semaphore_init(&consoleLock);
  DEBUG_TESTCODE("initAll1") { Semaphore_done(&consoleLock); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
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
  DEBUG_TESTCODE("initAll1") { Password_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Password_initAll,{ Password_doneAll(); });

  error = Compress_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll2") { Compress_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Compress_initAll,{ Compress_doneAll(); });

  error = Crypt_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll3") { Crypt_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Crypt_initAll,{ Crypt_doneAll(); });

  error = Pattern_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll4") { Pattern_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Pattern_initAll,{ Password_doneAll(); });

  error = PatternList_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll5") { PatternList_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,PatternList_initAll,{ PatternList_doneAll(); });

  error = Chunk_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll6") { Chunk_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Chunk_initAll,{ Chunk_doneAll(); });

  error = DeltaSource_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll7") { DeltaSource_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,DeltaSource_initAll,{ DeltaSource_doneAll(); });

  error = Archive_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll8") { Archive_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Archive_initAll,{ Archive_doneAll(); });

  error = Storage_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll9") { Storage_doneAll(), AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Storage_initAll,{ Storage_doneAll(); });

  error = Index_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll10") { Index_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Index_initAll,{ Index_doneAll(); });

  error = Continuous_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll11") { Continuous_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Continuous_initAll,{ Continuous_doneAll(); });

  error = Network_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll12") { Network_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Network_initAll,{ Network_doneAll(); });

  error = Server_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("initAll13") { Server_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Server_initAll,{ Server_doneAll(); });

  // initialize variables
  initGlobalOptions();

  command                               = COMMAND_LIST;
  jobName                               = NULL;
  uuid                                  = NULL;
  storageName                           = NULL;
  initJobOptions(&jobOptions);

  List_init(&serverList);
  Semaphore_init(&serverListLock);

  List_init(&deviceList);
  EntryList_init(&includeEntryList);
  PatternList_init(&excludePatternList);
  PatternList_init(&compressExcludePatternList);
  DeltaSourceList_init(&deltaSourceList);
  initServer(&defaultFTPServer,NULL,SERVER_TYPE_FTP);
  initServer(&defaultSSHServer,NULL,SERVER_TYPE_SSH);
  initServer(&defaultWebDAVServer,NULL,SERVER_TYPE_WEBDAV);
  defaultDevice.requestVolumeCommand     = NULL;
  defaultDevice.unloadVolumeCommand      = NULL;
  defaultDevice.loadVolumeCommand        = NULL;
  defaultDevice.volumeSize               = 0LL;
  defaultDevice.imagePreProcessCommand   = NULL;
  defaultDevice.imagePostProcessCommand  = NULL;
  defaultDevice.imageCommand             = NULL;
  defaultDevice.eccPreProcessCommand     = NULL;
  defaultDevice.eccPostProcessCommand    = NULL;
  defaultDevice.eccCommand               = NULL;
  defaultDevice.writePreProcessCommand   = NULL;
  defaultDevice.writePostProcessCommand  = NULL;
  defaultDevice.writeCommand             = NULL;
  currentFTPServer                       = &defaultFTPServer;
  currentSSHServer                       = &defaultSSHServer;
  currentDevice                          = &defaultDevice;
  daemonFlag                             = FALSE;
  noDetachFlag                           = FALSE;
  serverPort                             = DEFAULT_SERVER_PORT;
  serverTLSPort                          = DEFAULT_TLS_SERVER_PORT;
  serverCAFileName                       = DEFAULT_TLS_SERVER_CA_FILE;
  serverCertFileName                     = DEFAULT_TLS_SERVER_CERTIFICATE_FILE;
  serverKeyFileName                      = DEFAULT_TLS_SERVER_KEY_FILE;
  serverPassword                         = Password_new();
  serverJobsDirectory                    = DEFAULT_JOBS_DIRECTORY;

  indexDatabaseFileName                  = NULL;

  logTypes                               = 0;
  logFileName                            = NULL;
  logPostCommand                         = NULL;

  batchFlag                              = FALSE;
  versionFlag                            = FALSE;
  helpFlag                               = FALSE;
  xhelpFlag                              = FALSE;
  helpInternalFlag                       = FALSE;

  pidFileName                            = NULL;

  keyFileName                            = NULL;
  keyBits                                = MIN_ASYMMETRIC_CRYPT_KEY_BITS;

  StringList_init(&configFileNameList);

  tmpDirectory                           = String_new();
  tmpLogFileName                         = String_new();
  Semaphore_init(&logLock);
  logFile                                = NULL;
  tmpLogFile                             = NULL;
  POSIXLocale                            = newlocale(LC_ALL,"POSIX",0);

  Thread_initLocalVariable(&outputLineHandle,outputLineInit,NULL);
  lastOutputLine                         = NULL;

  indexHandle                            = NULL;

  // initialize default ssh keys
  fileName = String_new();
  File_appendFileNameCString(String_setCString(fileName,getenv("HOME")),".ssh/id_rsa.pub");
  if (File_exists(fileName))
  {
    readKeyFile(&defaultSSHServer.sshServer.publicKey,String_cString(fileName));
    readKeyFile(&defaultWebDAVServer.webDAVServer.publicKey,String_cString(fileName));
  }
  File_appendFileNameCString(String_setCString(fileName,getenv("HOME")),".ssh/id_rsa");
  if (File_exists(fileName))
  {
    readKeyFile(&defaultSSHServer.sshServer.privateKey,String_cString(fileName));
    readKeyFile(&defaultWebDAVServer.webDAVServer.privateKey,String_cString(fileName));
  }
  String_delete(fileName);

  // initialize command line options and config values
  ConfigValue_init(CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES));
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
  // deinitialize command line options and config values
  CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  ConfigValue_done(CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES));

  // deinitialize variables
  freelocale(POSIXLocale);
  Semaphore_done(&logLock);
  if (defaultDevice.writeCommand != NULL) String_delete(defaultDevice.writeCommand);
  if (defaultDevice.writePostProcessCommand != NULL) String_delete(defaultDevice.writePostProcessCommand);
  if (defaultDevice.writePreProcessCommand != NULL) String_delete(defaultDevice.writePreProcessCommand);
  if (defaultDevice.eccCommand != NULL) String_delete(defaultDevice.eccCommand);
  if (defaultDevice.eccPostProcessCommand != NULL) String_delete(defaultDevice.eccPostProcessCommand);
  if (defaultDevice.eccPreProcessCommand != NULL) String_delete(defaultDevice.eccPreProcessCommand);
  if (defaultDevice.imageCommand != NULL) String_delete(defaultDevice.imageCommand);
  if (defaultDevice.imagePostProcessCommand != NULL) String_delete(defaultDevice.imagePostProcessCommand);
  if (defaultDevice.imagePreProcessCommand != NULL) String_delete(defaultDevice.imagePreProcessCommand);
  if (defaultDevice.unloadVolumeCommand != NULL) String_delete(defaultDevice.unloadVolumeCommand);
  if (defaultDevice.loadVolumeCommand != NULL) String_delete(defaultDevice.loadVolumeCommand);
  if (defaultDevice.requestVolumeCommand != NULL) String_delete(defaultDevice.requestVolumeCommand);
  doneServer(&defaultWebDAVServer);
  doneServer(&defaultSSHServer);
  doneServer(&defaultFTPServer);
  DeltaSourceList_done(&deltaSourceList);
  PatternList_done(&compressExcludePatternList);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  Password_delete(serverPassword);
  List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);

  Semaphore_done(&serverListLock);
  List_done(&serverList,(ListNodeFreeFunction)freeServerNode,NULL);

  doneJobOptions(&jobOptions);
  String_delete(storageName);
  String_delete(uuid);
  String_delete(jobName);
  doneGlobalOptions();
  Thread_doneLocalVariable(&outputLineHandle,outputLineDone,NULL);
  String_delete(tmpLogFileName);
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
  Crypt_doneAll();
  Compress_doneAll();
  Password_doneAll();

  Semaphore_done(&consoleLock);

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
  if (!File_exists(globalOptions.tmpDirectory)) { printError("Temporary directory '%s' does not exists!\n",String_cString(globalOptions.tmpDirectory)); return FALSE; }
  if (!File_isDirectory(globalOptions.tmpDirectory)) { printError("'%s' is not a directory!\n",String_cString(globalOptions.tmpDirectory)); return FALSE; }
  if (!File_isWriteable(globalOptions.tmpDirectory)) { printError("Temporary directory '%s' is not writeable!\n",String_cString(globalOptions.tmpDirectory)); return FALSE; }

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

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
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

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    if (logFile != NULL)
    {
      fclose(logFile);
      logFile = NULL;
    }
  }
}

/***********************************************************************\
* Name   : openSessionLog
* Purpose: open session log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void openSessionLog(void)
{
  SemaphoreLock semaphoreLock;

  assert(tmpLogFileName != NULL);
  assert(tmpDirectory != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    File_setFileName(tmpLogFileName,tmpDirectory);
    File_appendFileNameCString(tmpLogFileName,"log.txt");
    tmpLogFile = fopen(String_cString(tmpLogFileName),"w");
  }
}

/***********************************************************************\
* Name   : closeSessionLog
* Purpose: close session log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void closeSessionLog(void)
{
  SemaphoreLock semaphoreLock;

  assert(tmpLogFile != NULL);
  assert(tmpLogFileName != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    fclose(tmpLogFile);
    tmpLogFile = NULL;
    File_delete(tmpLogFileName,FALSE);
  }
}

/*---------------------------------------------------------------------*/

bool isPrintInfo(uint verboseLevel)
{
  return !globalOptions.quietFlag && ((uint)globalOptions.verboseLevel >= verboseLevel);
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
    SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
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

void vlogMessage(ulong logType, const char *prefix, const char *text, va_list arguments)
{
  SemaphoreLock semaphoreLock;
  String        dateTime;
  va_list       tmpArguments;

  assert(text != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    if ((tmpLogFile != NULL) || (logFile != NULL))
    {
      if ((logType == LOG_TYPE_ALWAYS) || ((logTypes & logType) != 0))
      {
        dateTime = Misc_formatDateTime(String_new(),Misc_getCurrentDateTime(),NULL);

        if (tmpLogFile != NULL)
        {
          // append to temporary log file
          (void)fprintf(tmpLogFile,"%s> ",String_cString(dateTime));
          if (prefix != NULL)
          {
            (void)fputs(prefix,tmpLogFile);
            (void)fprintf(tmpLogFile,": ");
          }
          va_copy(tmpArguments,arguments);
          (void)vfprintf(tmpLogFile,text,tmpArguments);
          va_end(tmpArguments);
          fflush(tmpLogFile);
        }

        if (logFile != NULL)
        {
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

void plogMessage(ulong logType, const char *prefix, const char *text, ...)
{
  va_list arguments;

  assert(text != NULL);

  va_start(arguments,text);
  vlogMessage(logType,prefix,text,arguments);
  va_end(arguments);
}

void logMessage(ulong logType, const char *text, ...)
{
  va_list arguments;

  assert(text != NULL);

  va_start(arguments,text);
  vlogMessage(logType,NULL,text,arguments);
  va_end(arguments);
}

bool lockConsole(void)
{
  return Semaphore_lock(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,SEMAPHORE_WAIT_FOREVER);
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
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
  vlogMessage(LOG_TYPE_WARNING,"Warning",text,arguments);
  va_end(arguments);

  // output line
  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"Warning: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
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
  vlogMessage(LOG_TYPE_ERROR,"ERROR",text,arguments);
  va_end(arguments);

  // output line
  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"ERROR: ");
  String_vformat(line,text,arguments);
  va_end(arguments);
  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    saveConsole(stderr,&saveLine);
    (void)fwrite(String_cString(line),1,String_length(line),stderr);
    restoreConsole(stderr,&saveLine);
  }
  String_delete(line);
}

/***********************************************************************\
* Name   : executeIOOutput
* Purpose: process exec stdout, stderr output
* Input  : userData - string list or NULL
*          line     - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void executeIOOutput(void        *userData,
                     ConstString line
                    )
{
  StringList *stringList = (StringList*)userData;

  assert(line != NULL);

  printInfo(4,"%s\n",String_cString(line));
  if (stringList != NULL) StringList_append(stringList,line);
}

/***********************************************************************\
* Name   : executeIOlogPostProcess
* Purpose: process log-post command stderr output
* Input  : userData - strerr string list
*          line     - line
* Output : -
* Return : -
* Notes  : string list will be shortend to last 5 entries
\***********************************************************************/

LOCAL void executeIOlogPostProcess(void        *userData,
                                   ConstString line
                                  )
{
  StringList *stringList = (StringList*)userData;

  assert(stringList != NULL);
  assert(line != NULL);

  StringList_append(stringList,line);
  while (StringList_count(stringList) > 5)
  {
    String_delete(StringList_getFirst(stringList,NULL));
  }
}

void logPostProcess(void)
{
  SemaphoreLock semaphoreLock;
  TextMacro     textMacros[1];
  StringList    stderrList;
  Errors        error;
  StringNode    *stringNode;
  String        string;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // flush log
    if (logFile != NULL) fflush(logFile);

    // close temporary log file
    if (tmpLogFile != NULL) fclose(tmpLogFile); tmpLogFile = NULL;

    // log post command for temporary log file
    if (logPostCommand != NULL)
    {
      printInfo(2,"Log post process '%s'...",logPostCommand);

      TEXT_MACRO_N_STRING(textMacros[0],"%file",tmpLogFileName);

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

    // reset and reopen temporary log file
    tmpLogFile = fopen(String_cString(tmpLogFileName),"w");
  }
}

void initJobOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  memset(jobOptions,0,sizeof(JobOptions));
  jobOptions->archiveType                     = ARCHIVE_TYPE_NORMAL;
  jobOptions->archivePartSize                 = 0LL;
  jobOptions->directoryStripCount             = DIRECTORY_STRIP_NONE;
  jobOptions->owner.userId                    = FILE_DEFAULT_USER_ID;
  jobOptions->owner.groupId                   = FILE_DEFAULT_GROUP_ID;
  jobOptions->patternType                     = PATTERN_TYPE_GLOB;
  jobOptions->compressAlgorithms.delta        = COMPRESS_ALGORITHM_NONE;
  jobOptions->compressAlgorithms.byte         = COMPRESS_ALGORITHM_NONE;
  jobOptions->cryptAlgorithm                  = CRYPT_ALGORITHM_NONE;
  #ifdef HAVE_GCRYPT
    jobOptions->cryptType                     = CRYPT_TYPE_SYMMETRIC;
  #else /* not HAVE_GCRYPT */
    jobOptions->cryptType                     = CRYPT_TYPE_NONE;
  #endif /* HAVE_GCRYPT */
  jobOptions->cryptPasswordMode               = PASSWORD_MODE_DEFAULT;
  jobOptions->cryptPublicKeyFileName          = NULL;
  jobOptions->cryptPrivateKeyFileName         = NULL;
  jobOptions->mountDeviceName                 = NULL;
  jobOptions->preProcessCommand               = NULL;
  jobOptions->postProcessCommand              = NULL;
  jobOptions->maxStorageSize                  = 0LL;
  jobOptions->volumeSize                      = 0LL;
  jobOptions->skipUnreadableFlag              = TRUE;
  jobOptions->forceDeltaCompressionFlag       = FALSE;
  jobOptions->ignoreNoDumpAttributeFlag       = FALSE;
  jobOptions->archiveFileMode                 = ARCHIVE_FILE_MODE_STOP;
  jobOptions->overwriteFilesFlag              = FALSE;
  jobOptions->errorCorrectionCodesFlag        = FALSE;
  jobOptions->alwaysCreateImageFlag           = FALSE;
  jobOptions->waitFirstVolumeFlag             = FALSE;
  jobOptions->rawImagesFlag                   = FALSE;
  jobOptions->noFragmentsCheckFlag            = FALSE;
  jobOptions->noIndexDatabaseFlag             = FALSE;
  jobOptions->dryRunFlag                      = FALSE;
  jobOptions->noStorageFlag                   = FALSE;
  jobOptions->noBAROnMediumFlag               = FALSE;
  jobOptions->stopOnErrorFlag                 = FALSE;
}

void initDuplicateJobOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions)
{
  assert(jobOptions != NULL);
  assert(fromJobOptions != NULL);

  initJobOptions(jobOptions);
  copyJobOptions(fromJobOptions,jobOptions);
}

void copyJobOptions(const JobOptions *fromJobOptions, JobOptions *toJobOptions)
{
  assert(fromJobOptions != NULL);
  assert(toJobOptions != NULL);

  memcpy(toJobOptions,fromJobOptions,sizeof(JobOptions));

  toJobOptions->incrementalListFileName             = String_duplicate(fromJobOptions->incrementalListFileName);
  toJobOptions->destination                         = String_duplicate(fromJobOptions->destination);

  toJobOptions->cryptPassword                       = Password_duplicate(fromJobOptions->cryptPassword);
  toJobOptions->cryptPublicKeyFileName              = String_duplicate(fromJobOptions->cryptPublicKeyFileName);
  toJobOptions->cryptPrivateKeyFileName             = String_duplicate(fromJobOptions->cryptPrivateKeyFileName);

  toJobOptions->mountDeviceName                     = String_duplicate(fromJobOptions->mountDeviceName);

  toJobOptions->preProcessCommand                   = String_duplicate(fromJobOptions->preProcessCommand);
  toJobOptions->postProcessCommand                  = String_duplicate(fromJobOptions->postProcessCommand);

  toJobOptions->ftpServer.loginName                 = String_duplicate(fromJobOptions->ftpServer.loginName);
  toJobOptions->ftpServer.password                  = Password_duplicate(fromJobOptions->ftpServer.password);

  toJobOptions->sshServer.loginName                 = String_duplicate(fromJobOptions->sshServer.loginName);
  toJobOptions->sshServer.password                  = Password_duplicate(fromJobOptions->sshServer.password);
  duplicateKey(&toJobOptions->sshServer.publicKey,&fromJobOptions->sshServer.publicKey);
  duplicateKey(&toJobOptions->sshServer.privateKey,&fromJobOptions->sshServer.privateKey);

  toJobOptions->webDAVServer.loginName              = String_duplicate(fromJobOptions->webDAVServer.loginName);
  toJobOptions->webDAVServer.password               = Password_duplicate(fromJobOptions->webDAVServer.password);
  duplicateKey(&toJobOptions->webDAVServer.publicKey,&fromJobOptions->webDAVServer.publicKey);
  duplicateKey(&toJobOptions->webDAVServer.privateKey,&fromJobOptions->webDAVServer.privateKey);

  toJobOptions->opticalDisk.requestVolumeCommand    = String_duplicate(fromJobOptions->opticalDisk.requestVolumeCommand);
  toJobOptions->opticalDisk.unloadVolumeCommand     = String_duplicate(fromJobOptions->opticalDisk.unloadVolumeCommand);
  toJobOptions->opticalDisk.imagePreProcessCommand  = String_duplicate(fromJobOptions->opticalDisk.imagePreProcessCommand);
  toJobOptions->opticalDisk.imagePostProcessCommand = String_duplicate(fromJobOptions->opticalDisk.imagePostProcessCommand);
  toJobOptions->opticalDisk.imageCommand            = String_duplicate(fromJobOptions->opticalDisk.imageCommand);
  toJobOptions->opticalDisk.eccPreProcessCommand    = String_duplicate(fromJobOptions->opticalDisk.eccPreProcessCommand);
  toJobOptions->opticalDisk.eccPostProcessCommand   = String_duplicate(fromJobOptions->opticalDisk.eccPostProcessCommand);
  toJobOptions->opticalDisk.eccCommand              = String_duplicate(fromJobOptions->opticalDisk.eccCommand);
  toJobOptions->opticalDisk.writePreProcessCommand  = String_duplicate(fromJobOptions->opticalDisk.writePreProcessCommand);
  toJobOptions->opticalDisk.writePostProcessCommand = String_duplicate(fromJobOptions->opticalDisk.writePostProcessCommand);
  toJobOptions->opticalDisk.writeCommand            = String_duplicate(fromJobOptions->opticalDisk.writeCommand);

  toJobOptions->deviceName                          = String_duplicate(fromJobOptions->deviceName);
  toJobOptions->device.requestVolumeCommand         = String_duplicate(fromJobOptions->device.requestVolumeCommand);
  toJobOptions->device.unloadVolumeCommand          = String_duplicate(fromJobOptions->device.unloadVolumeCommand);
  toJobOptions->device.imagePreProcessCommand       = String_duplicate(fromJobOptions->device.imagePreProcessCommand);
  toJobOptions->device.imagePostProcessCommand      = String_duplicate(fromJobOptions->device.imagePostProcessCommand);
  toJobOptions->device.imageCommand                 = String_duplicate(fromJobOptions->device.imageCommand);
  toJobOptions->device.eccPreProcessCommand         = String_duplicate(fromJobOptions->device.eccPreProcessCommand);
  toJobOptions->device.eccPostProcessCommand        = String_duplicate(fromJobOptions->device.eccPostProcessCommand);
  toJobOptions->device.eccCommand                   = String_duplicate(fromJobOptions->device.eccCommand);
  toJobOptions->device.writePreProcessCommand       = String_duplicate(fromJobOptions->device.writePreProcessCommand);
  toJobOptions->device.writePostProcessCommand      = String_duplicate(fromJobOptions->device.writePostProcessCommand);
  toJobOptions->device.writeCommand                 = String_duplicate(fromJobOptions->device.writeCommand);
}

void doneJobOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  String_delete(jobOptions->device.writeCommand);
  String_delete(jobOptions->device.writePostProcessCommand);
  String_delete(jobOptions->device.writePreProcessCommand);
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

  String_delete(jobOptions->postProcessCommand);
  String_delete(jobOptions->preProcessCommand);

  String_delete(jobOptions->mountDeviceName);

  String_delete(jobOptions->cryptPrivateKeyFileName);
  String_delete(jobOptions->cryptPublicKeyFileName);
  Password_delete(jobOptions->cryptPassword);

  String_delete(jobOptions->destination);
  String_delete(jobOptions->incrementalListFileName);

  memset(jobOptions,0,sizeof(JobOptions));
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
          File_close(&fileHandle);

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

void initKey(Key *key)
{
  assert(key != NULL);

  key->data   = NULL;
  key->length = 0;
}

bool duplicateKey(Key *toKey, const Key *fromKey)
{
  void *data;

  assert(toKey != NULL);
  assert(fromKey != NULL);

  data = Password_allocSecure(fromKey->length);
  if (data == NULL)
  {
    return FALSE;
  }
  memcpy(data,fromKey->data,fromKey->length);

  toKey->data   = data;
  toKey->length = fromKey->length;

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

void clearKey(Key *key)
{
  assert(key != NULL);

  if (key->data != NULL) Password_freeSecure(key->data);
  key->data   = NULL;
  key->length = 0;
}

bool isKeyAvailable(const Key *key)
{
  assert(key != NULL);

  return key->data != NULL;
}

Errors readKeyFile(Key *key, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  uint       length;
  void       *data;

  assert(key != NULL);
  assert(fileName != NULL);

  key->data   = NULL;
  key->length = 0;

  error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get file size
  length = (uint)File_getSize(&fileHandle);

  // allocate secure memory
  data = Password_allocSecure(length);
  if (data == NULL)
  {
    (void)File_close(&fileHandle);
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // read file data
  error = File_read(&fileHandle,
                    data,
                    length,
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

  // store data
  if (key->data != NULL) Password_freeSecure(key->data);
  key->data   = data;
  key->length = length;

  return ERROR_NONE;
}

bool allocateServer(Server *server, ServerConnectionPriorities priority, long timeout)
{
  SemaphoreLock semaphoreLock;
  uint          maxConnectionCount;

  assert(server != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&server->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // get max. number of allowed concurrent connections
    if (server->maxConnectionCount != MAX_CONNECTION_COUNT_UNLIMITED)
    {
      maxConnectionCount = server->maxConnectionCount;
    }
    else
    {
      maxConnectionCount = 0;
      switch (server->type)
      {
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
        if (   (maxConnectionCount != MAX_CONNECTION_COUNT_UNLIMITED)
            && (server->connection.count >= maxConnectionCount)
           )
        {
          // request low priority connection
          server->connection.lowPriorityRequestCount++;

          // wait for free connection
          while (server->connection.count >= maxConnectionCount)
          {
            if (!Semaphore_waitModified(&server->lock,timeout))
            {
              Semaphore_unlock(&server->lock);
              return FALSE;
            }
          }

          // low priority request done
          assert(server->connection.lowPriorityRequestCount > 0);
          server->connection.lowPriorityRequestCount--;
        }
        break;
      case SERVER_CONNECTION_PRIORITY_HIGH:
        if (   (maxConnectionCount != MAX_CONNECTION_COUNT_UNLIMITED)
            && (server->connection.count >= maxConnectionCount)
           )
        {
          // request high priority connection
          server->connection.highPriorityRequestCount++;

          // wait for free connection
          while (server->connection.count >= maxConnectionCount)
          {
            if (!Semaphore_waitModified(&server->lock,timeout))
            {
              Semaphore_unlock(&server->lock);
              return FALSE;
            }
          }

          // high priority request done
          assert(server->connection.highPriorityRequestCount > 0);
          server->connection.highPriorityRequestCount--;
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }

    // allocated connection
    server->connection.count++;
  }

  return TRUE;
}

void freeServer(Server *server)
{
  SemaphoreLock semaphoreLock;

  assert(server != NULL);
  assert(server->connection.count > 0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&server->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // free connection
    server->connection.count--;
  }
}

bool isServerAllocationPending(Server *server)
{
  SemaphoreLock semaphoreLock;
  bool          pendingFlag;

  assert(server != NULL);

  pendingFlag = FALSE;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&server->lock,SEMAPHORE_LOCK_TYPE_READ)
  {
    pendingFlag = (server->connection.highPriorityRequestCount > 0);
  }

  return pendingFlag;
}

Server *getFTPServerSettings(ConstString      hostName,
                             const JobOptions *jobOptions,
                             FTPServer        *ftpServer
                            )
{
  ServerNode *serverNode;

  assert(hostName != NULL);
  assert(ftpServer != NULL);

  // find FTP server
  serverNode = globalOptions.serverList->head;
  while (   (serverNode != NULL)
         && (   (serverNode->server.type != SERVER_TYPE_FTP)
             || !String_equals(serverNode->server.name,hostName)
            )
        )
  {
    serverNode = serverNode->next;
  }

  // get FTP server settings
  ftpServer->loginName = ((jobOptions != NULL) && !String_isEmpty(jobOptions->ftpServer.loginName) ) ? jobOptions->ftpServer.loginName : ((serverNode != NULL) ? serverNode->server.ftpServer.loginName : globalOptions.defaultFTPServer->ftpServer.loginName);
  ftpServer->password  = ((jobOptions != NULL) && !Password_isEmpty(jobOptions->ftpServer.password)) ? jobOptions->ftpServer.password  : ((serverNode != NULL) ? serverNode->server.ftpServer.password  : globalOptions.defaultFTPServer->ftpServer.password );

  return (serverNode != NULL) ? &serverNode->server : &defaultFTPServer;
}

Server *getSSHServerSettings(ConstString      hostName,
                             const JobOptions *jobOptions,
                             SSHServer        *sshServer
                            )
{
  ServerNode *serverNode;

  assert(hostName != NULL);
  assert(sshServer != NULL);

  // find SSH server
  serverNode = globalOptions.serverList->head;
  while (   (serverNode != NULL)
         && (   (serverNode->server.type != SERVER_TYPE_SSH)
             || !String_equals(serverNode->server.name,hostName)
            )
        )
  {
    serverNode = serverNode->next;
  }

  // get SSH server settings
  sshServer->port       = ((jobOptions != NULL) && (jobOptions->sshServer.port != 0)                ) ? jobOptions->sshServer.port       : ((serverNode != NULL) ? serverNode->server.sshServer.port       : globalOptions.defaultSSHServer->sshServer.port      );
  sshServer->loginName  = ((jobOptions != NULL) && !String_isEmpty(jobOptions->sshServer.loginName) ) ? jobOptions->sshServer.loginName  : ((serverNode != NULL) ? serverNode->server.sshServer.loginName  : globalOptions.defaultSSHServer->sshServer.loginName );
  sshServer->password   = ((jobOptions != NULL) && !Password_isEmpty(jobOptions->sshServer.password)) ? jobOptions->sshServer.password   : ((serverNode != NULL) ? serverNode->server.sshServer.password   : globalOptions.defaultSSHServer->sshServer.password  );
  sshServer->publicKey  = ((jobOptions != NULL) && isKeyAvailable(&jobOptions->sshServer.publicKey) ) ? jobOptions->sshServer.publicKey  : ((serverNode != NULL) ? serverNode->server.sshServer.publicKey  : globalOptions.defaultSSHServer->sshServer.publicKey );
  sshServer->privateKey = ((jobOptions != NULL) && isKeyAvailable(&jobOptions->sshServer.privateKey)) ? jobOptions->sshServer.privateKey : ((serverNode != NULL) ? serverNode->server.sshServer.privateKey : globalOptions.defaultSSHServer->sshServer.privateKey);

  return (serverNode != NULL) ? &serverNode->server : &defaultSSHServer;
}

Server *getWebDAVServerSettings(ConstString      hostName,
                                const JobOptions *jobOptions,
                                WebDAVServer     *webDAVServer
                               )
{
  ServerNode *serverNode;

  assert(hostName != NULL);
  assert(webDAVServer != NULL);

  // find WebDAV server
  serverNode = globalOptions.serverList->head;
  while (   (serverNode != NULL)
         && (   (serverNode->server.type != SERVER_TYPE_WEBDAV)
             || !String_equals(serverNode->server.name,hostName)
            )
        )
  {
    serverNode = serverNode->next;
  }

  // get WebDAV server settings
//  webDAVServer->port       = ((jobOptions != NULL) && (jobOptions->webDAVServer.port != 0) ? jobOptions->webDAVServer.port : ((serverNode != NULL) ? serverNode->webDAVServer.port : globalOptions.defaultWebDAVServer->port );
  webDAVServer->loginName  = ((jobOptions != NULL) && !String_isEmpty(jobOptions->webDAVServer.loginName) ) ? jobOptions->webDAVServer.loginName  : ((serverNode != NULL) ? serverNode->server.webDAVServer.loginName  : globalOptions.defaultWebDAVServer->webDAVServer.loginName );
  webDAVServer->password   = ((jobOptions != NULL) && !Password_isEmpty(jobOptions->webDAVServer.password)) ? jobOptions->webDAVServer.password   : ((serverNode != NULL) ? serverNode->server.webDAVServer.password   : globalOptions.defaultWebDAVServer->webDAVServer.password  );
  webDAVServer->publicKey  = ((jobOptions != NULL) && isKeyAvailable(&jobOptions->webDAVServer.publicKey) ) ? jobOptions->webDAVServer.publicKey  : ((serverNode != NULL) ? serverNode->server.webDAVServer.publicKey  : globalOptions.defaultWebDAVServer->webDAVServer.publicKey );
  webDAVServer->privateKey = ((jobOptions != NULL) && isKeyAvailable(&jobOptions->webDAVServer.privateKey)) ? jobOptions->webDAVServer.privateKey : ((serverNode != NULL) ? serverNode->server.webDAVServer.privateKey : globalOptions.defaultWebDAVServer->webDAVServer.privateKey);

  return (serverNode != NULL) ? &serverNode->server : &defaultWebDAVServer;
}

void getCDSettings(const JobOptions *jobOptions,
                   OpticalDisk      *opticalDisk
                  )
{
  assert(opticalDisk != NULL);

  opticalDisk->defaultDeviceName       = ((jobOptions != NULL) && (jobOptions->opticalDisk.defaultDeviceName       != NULL)) ? jobOptions->opticalDisk.defaultDeviceName       : globalOptions.cd.defaultDeviceName;
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

  opticalDisk->defaultDeviceName       = ((jobOptions != NULL) && (jobOptions->opticalDisk.defaultDeviceName       != NULL)) ? jobOptions->opticalDisk.defaultDeviceName       : globalOptions.dvd.defaultDeviceName;
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

  opticalDisk->defaultDeviceName       = ((jobOptions != NULL) && (jobOptions->opticalDisk.defaultDeviceName       != NULL)) ? jobOptions->opticalDisk.defaultDeviceName       : globalOptions.bd.defaultDeviceName;
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
  DeviceNode *deviceNode;

  assert(name != NULL);
  assert(device != NULL);

  deviceNode = globalOptions.deviceList->head;
  while ((deviceNode != NULL) && !String_equals(deviceNode->name,name))
  {
    deviceNode = deviceNode->next;
  }
  device->defaultDeviceName       = ((jobOptions != NULL) && (jobOptions->device.defaultDeviceName       != NULL)) ? jobOptions->device.defaultDeviceName       : ((deviceNode != NULL) ? deviceNode->device.defaultDeviceName       : globalOptions.defaultDevice->defaultDeviceName      );
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
  device->writePreProcessCommand  = ((jobOptions != NULL) && (jobOptions->device.writePreProcessCommand  != NULL)) ? jobOptions->device.writePreProcessCommand  : ((deviceNode != NULL) ? deviceNode->device.writePreProcessCommand  : globalOptions.defaultDevice->writePreProcessCommand );
  device->writePostProcessCommand = ((jobOptions != NULL) && (jobOptions->device.writePostProcessCommand != NULL)) ? jobOptions->device.writePostProcessCommand : ((deviceNode != NULL) ? deviceNode->device.writePostProcessCommand : globalOptions.defaultDevice->writePostProcessCommand);
  device->writeCommand            = ((jobOptions != NULL) && (jobOptions->device.writeCommand            != NULL)) ? jobOptions->device.writeCommand            : ((deviceNode != NULL) ? deviceNode->device.writeCommand            : globalOptions.defaultDevice->writeCommand           );
}

Errors inputCryptPassword(void        *userData,
                          Password    *password,
                          ConstString fileName,
                          bool        validateFlag,
                          bool        weakCheckFlag
                         )
{
  Errors        error;
  SemaphoreLock semaphoreLock;

  assert(password != NULL);

  UNUSED_VARIABLE(userData);

  error = ERROR_UNKNOWN;

  switch (globalOptions.runMode)
  {
    case RUN_MODE_INTERACTIVE:
      {
        String title;
        String saveLine;

        SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          saveConsole(stdout,&saveLine);

          // input password
          title = String_new();
          if ((fileName != NULL) && !String_isEmpty(fileName))
          {
            String_format(title,"Crypt password for '%S'",fileName);
          }
          else
          {
            String_setCString(title,"Crypt password");
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
            if ((fileName != NULL) && !String_isEmpty(fileName))
            {
              String_format(String_clear(title),"Verify password for '%S'",fileName);
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
  const char *s;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  password = (Password*)(*formatUserData);
  if (password != NULL)
  {
    s = Password_deploy(password);
    String_format(line,"%'s",s);
    Password_undeploy(password);

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
    snprintf(errorMessage,errorMessageSize,"Cannot parse bandwidth value '%s'",value);
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
    snprintf(errorMessage,errorMessageSize,"Unknown owner ship value '%s'",value);
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

LOCAL bool configValueParseEntry(EntryTypes entryType, void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
  error = EntryList_append((EntryList*)variable,entryType,pattern,patternType);
  if (error != ERROR_NONE)
  {
    strncpy(errorMessage,Error_getText(error),errorMessageSize); errorMessage[errorMessageSize-1] = '\0';
    String_delete(pattern);
    return FALSE;
  }
  String_delete(pattern);

  return TRUE;
}

bool configValueParseFileEntry(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  return configValueParseEntry(ENTRY_TYPE_FILE,userData,variable,name,value,errorMessage,errorMessageSize);
}

bool configValueParseImageEntry(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  return configValueParseEntry(ENTRY_TYPE_IMAGE,userData,variable,name,value,errorMessage,errorMessageSize);
}

void configValueFormatInitEntry(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((EntryList*)variable)->head;
}

void configValueFormatDoneEntry(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatFileEntry(void **formatUserData, void *userData, String line)
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

bool configValueFormatImageEntry(void **formatUserData, void *userData, String line)
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
  error = PatternList_appendCString((PatternList*)variable,value,patternType);
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
      snprintf(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",algorithm1);
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
      snprintf(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",algorithm2);
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
      snprintf(errorMessage,errorMessageSize,"Unknown compress algorithm value '%s'",value);
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

bool configValueReadKeyFile(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  Key    *key = (Key*)variable;
  Errors error;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (File_existsCString(value))
  {
    error = readKeyFile(key,value);
    if (error != ERROR_NONE)
    {
      return FALSE;
    }
  }

  return TRUE;
}

bool configValueParseOverwriteArchiveFiles(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(ArchiveFileModes*)variable) = ARCHIVE_FILE_MODE_OVERWRITE;
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
                             CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES),
                             NULL, // sectionName,
                             NULL, // errorOutputHandle,
                             NULL, // errorPrefix,
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
      printError("Syntax error in %s, line %ld: '%s' - skipped\n",
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
  File_close(&fileHandle);

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

  if (pidFileName != NULL)
  {
    fileName = String_new();
    error = File_open(&fileHandle,File_setFileNameCString(fileName,pidFileName),FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      printError("Cannot create process id file '%s' (error: %s)\n",pidFileName,Error_getText(error));
      return error;
    }
    File_printLine(&fileHandle,"%d",(int)getpid());
    File_close(&fileHandle);
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
  String fileName;

  if (pidFileName != NULL)
  {
    fileName = String_newCString(pidFileName);
    File_delete(fileName,FALSE);
    String_delete(fileName);
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
  Errors error;
  String fileName;
  bool   printInfoFlag;

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
  globalOptions.barExecutable = argv[0];

#if 0
{
CryptKey p,s;
String n,e;

Crypt_createKeys(&p,&s,1024);
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
                       1,
                       stderr,"ERROR: "
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

  // read all configuration files
  fileName = String_new();
  printInfoFlag = daemonFlag;
  while (StringList_getFirst(&configFileNameList,fileName) != NULL)
  {
    if (!readConfigFile(fileName,printInfoFlag))
    {
      String_delete(fileName);
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
  String_delete(fileName);

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
                       CMD_PRIORITY_ANY,
                       stderr,"ERROR: "
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
    printError("Cannot create temporary directory in '%s' (error: %s)!\n",String_cString(globalOptions.tmpDirectory),Error_getText(error));
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

  error = ERROR_NONE;
  if      (daemonFlag)
  {
    // open log file, create session log file
    openLog();
    openSessionLog();

    if (!stringIsEmpty(indexDatabaseFileName))
    {
      // open index database
      printInfo(1,"Opening index database '%s'...",indexDatabaseFileName);
      error = Index_init(&__indexHandle,indexDatabaseFileName);
      if (error != ERROR_NONE)
      {
        if (printInfoFlag) printf("FAIL!\n");
        printError("Cannot open index database '%s' (error: %s)!\n",
                   indexDatabaseFileName,
                   Error_getText(error)
                  );
        // close log files
        closeSessionLog();
        closeLog();
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
      indexHandle = &__indexHandle;
      printInfo(1,"ok\n");
    }

    // daemon mode -> run server with network
    globalOptions.runMode = RUN_MODE_SERVER;

    // create pid file
    error = createPIDFile();
    if (error == ERROR_NONE)
    {
      // run server (not detached)
      error = Server_run(serverPort,
                         serverTLSPort,
                         serverCAFileName,
                         serverCertFileName,
                         serverKeyFileName,
                         serverPassword,
                         serverJobsDirectory,
                         &jobOptions
                        );
      // delete pid file
      deletePIDFile();
    }

    // close index database
    if (indexHandle != NULL) Index_done(indexHandle);

    // close log files
    closeSessionLog();
    closeLog();
  }
  else if (batchFlag)
  {
    // batch mode
    globalOptions.runMode = RUN_MODE_BATCH;

    // batch mode -> run server with standard i/o
    error = Server_batch(STDIN_FILENO,
                         STDOUT_FILENO
                        );
  }
  else if (jobName != NULL)
  {
    // start job execution
    globalOptions.runMode = RUN_MODE_INTERACTIVE;

    // create archive
    error = Command_create(NULL, // job UUID
                           NULL, // schedule UUID
                           storageName,
                           &includeEntryList,
                           &excludePatternList,
                           &compressExcludePatternList,
                           &deltaSourceList,
                           &jobOptions,
                           ARCHIVE_TYPE_NORMAL,
                           NULL, // scheduleTitle
                           NULL, // scheduleCustomText
                           CALLBACK(inputCryptPassword,NULL),
                           CALLBACK(NULL,NULL), // createStatusInfoFunction
                           CALLBACK(NULL,NULL), // storageRequestVolumeFunction
                           NULL, // pauseCreateFlag
                           NULL, // pauseStorageFlag
                           NULL  // requestedAbortFlag
                          );
  }
  else
  {
    // interactive mode
    globalOptions.runMode = RUN_MODE_INTERACTIVE;

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
              error = EntryList_appendCString(&includeEntryList,entryType,argv[z],jobOptions.patternType);
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
                                  storageName,
                                  &includeEntryList,
                                  &excludePatternList,
                                  &compressExcludePatternList,
                                  &deltaSourceList,
                                  &jobOptions,
                                  ARCHIVE_TYPE_NORMAL,
                                  NULL, // scheduleTitle
                                  NULL, // scheduleCustomText
                                  CALLBACK(inputCryptPassword,NULL),
                                  CALLBACK(NULL,NULL), // createStatusInfoFunction
                                  CALLBACK(NULL,NULL), // storageRequestVolumeFunction
                                  NULL, // pauseCreateFlag
                                  NULL, // pauseStorageFlag
                                  NULL  // requestedAbortFlag
                                  );
          }

          // free resources
          String_delete(storageName);
        }
        break;
      case COMMAND_LIST:
      case COMMAND_TEST:
      case COMMAND_COMPARE:
      case COMMAND_RESTORE:
        {
          StringList fileNameList;
          int        z;

          // get archive files
          StringList_init(&fileNameList);
          for (z = 1; z < argc; z++)
          {
            StringList_appendCString(&fileNameList,argv[z]);
          }

          switch (command)
          {
            case COMMAND_LIST:
              error = Command_list(&fileNameList,
                                   &includeEntryList,
                                   &excludePatternList,
                                   &jobOptions,
                                   CALLBACK(inputCryptPassword,NULL)
                                  );
              break;
            case COMMAND_TEST:
              error = Command_test(&fileNameList,
                                   &includeEntryList,
                                   &excludePatternList,
                                   &deltaSourceList,
                                   &jobOptions,
                                   CALLBACK(inputCryptPassword,NULL)
                                  );
              break;
            case COMMAND_COMPARE:
              error = Command_compare(&fileNameList,
                                      &includeEntryList,
                                      &excludePatternList,
                                      &deltaSourceList,
                                      &jobOptions,
                                      CALLBACK(inputCryptPassword,NULL)
                                     );
              break;
            case COMMAND_RESTORE:
              error = Command_restore(&fileNameList,
                                      &includeEntryList,
                                      &excludePatternList,
                                      &deltaSourceList,
                                      &jobOptions,
                                      CALLBACK(inputCryptPassword,NULL),
                                      CALLBACK(NULL,NULL),  // restoreStatusInfoUserData callback
                                      NULL,  // pauseRestoreFlag
                                      NULL  // requestedAbortFlag
                                     );
              break;
            default:
              break;
          }

          // free resources
          StringList_done(&fileNameList);
        }
        break;
      case COMMAND_GENERATE_KEYS:
        {
          // generate new key pair
          const char *keyFileName;
          Password   cryptPassword;
          CryptKey   publicKey,privateKey;
          String     publicKeyFileName,privateKeyFileName;

          // get key file name
          if (argc <= 1)
          {
            printError("No key file name given!\n");
            printUsage(argv[0],0);
            error = ERROR_INVALID_ARGUMENT;
            break;
          }
          keyFileName = argv[1];

          // initialize variables
          publicKeyFileName  = String_new();
          privateKeyFileName = String_new();

          // get file names of keys
          File_setFileNameCString(publicKeyFileName,keyFileName);
          String_appendCString(publicKeyFileName,".public");
          File_setFileNameCString(privateKeyFileName,keyFileName);
          String_appendCString(privateKeyFileName,".private");

          // check if key files already exists
          if (File_exists(publicKeyFileName))
          {
            printError("Public key file '%s' already exists!\n",String_cString(publicKeyFileName));
            String_delete(privateKeyFileName);
            String_delete(publicKeyFileName);
            break;
          }
          if (File_exists(privateKeyFileName))
          {
            printError("Private key file '%s' already exists!\n",String_cString(privateKeyFileName));
            String_delete(privateKeyFileName);
            String_delete(publicKeyFileName);
            break;
          }

          // input crypt password for private key encryption
          Password_init(&cryptPassword);
          error = inputCryptPassword(NULL,&cryptPassword,privateKeyFileName,TRUE,FALSE);
          if (error != ERROR_NONE)
          {
            printError("No password given for private key!\n");
            Password_done(&cryptPassword);
            String_delete(privateKeyFileName);
            String_delete(publicKeyFileName);
            break;
          }

          // generate new keys pair
          error = Crypt_createKeys(&publicKey,&privateKey,keyBits,CRYPT_PADDING_TYPE_NONE);
          if (error != ERROR_NONE)
          {
            printError("Cannot create key pair (error: %s)!\n",Error_getText(error));
            Crypt_doneKey(&privateKey);
            Crypt_doneKey(&publicKey);
            Password_done(&cryptPassword);
            String_delete(privateKeyFileName);
            String_delete(publicKeyFileName);
            break;
          }
          error = Crypt_writeKeyFile(&publicKey,publicKeyFileName,NULL);
          if (error != ERROR_NONE)
          {
            printError("Cannot write public key file!\n");
            Crypt_doneKey(&privateKey);
            Crypt_doneKey(&publicKey);
            Password_done(&cryptPassword);
            String_delete(privateKeyFileName);
            String_delete(publicKeyFileName);
            break;
          }
          error = Crypt_writeKeyFile(&privateKey,privateKeyFileName,&cryptPassword);
          if (error != ERROR_NONE)
          {
            printError("Cannot write private key file!\n");
            Crypt_doneKey(&privateKey);
            Crypt_doneKey(&publicKey);
            Password_done(&cryptPassword);
            String_delete(privateKeyFileName);
            String_delete(publicKeyFileName);
            break;
          }
          // free resources
          Crypt_doneKey(&privateKey);
          Crypt_doneKey(&publicKey);
          Password_done(&cryptPassword);
          String_delete(privateKeyFileName);
          String_delete(publicKeyFileName);
        }
        break;
      default:
        printError("No command given!\n");
        error = ERROR_INVALID_ARGUMENT;
        break;
    }
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

  // parse command line: pre-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,
                       stderr,"ERROR: "
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
    return ERROR_INVALID_ARGUMENT;
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
