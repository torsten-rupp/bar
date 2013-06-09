/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver main program
* Systems: all
*
\***********************************************************************/

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
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "global.h"
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
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "archive.h"
#include "network.h"
#include "storage.h"
#include "sources.h"
#include "database.h"
#include "index.h"
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
#define VERSION_STRING VERSION_MAJOR_STRING "." VERSION_MINOR_STRING

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

#define DEFAULT_DATABASE_INDEX_FILE           "/var/lib/bar/index.db"

#define CD_UNLOAD_VOLUME_COMMAND              "eject -r %device"
#define CD_LOAD_VOLUME_COMMAND                "eject -t %device"
#define CD_IMAGE_COMMAND                      "nice mkisofs -V Backup -volset %number -r -o %image %directory"
#define CD_ECC_COMMAND                        "nice dvdisaster -mRS02 -n cd -c -i %image -v"
#define CD_WRITE_COMMAND                      "nice sh -c 'mkisofs -V Backup -volset %number -r -o %image %directory && cdrecord dev=%device %image'"
#define CD_WRITE_IMAGE_COMMAND                "nice cdrecord dev=%device %image"

#define DVD_UNLOAD_VOLUME_COMMAND             "eject -r %device"
#define DVD_LOAD_VOLUME_COMMAND               "eject -t %device"
#define DVD_IMAGE_COMMAND                     "nice mkisofs -V Backup -volset %number -r -o %image %directory"
#define DVD_ECC_COMMAND                       "nice dvdisaster -mRS02 -n dvd -c -i %image -v"
#define DVD_WRITE_COMMAND                     "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %directory"
#define DVD_WRITE_IMAGE_COMMAND               "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload"

#define BD_UNLOAD_VOLUME_COMMAND              "eject -r %device"
#define BD_LOAD_VOLUME_COMMAND                "eject -t %device"
#define BD_IMAGE_COMMAND                      "nice mkisofs -V Backup -volset %number -r -o %image %directory"
#define BD_ECC_COMMAND                        "nice dvdisaster -mRS02 -n bd -c -i %image -v"
#define BD_WRITE_COMMAND                      "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %directory"
#define BD_WRITE_IMAGE_COMMAND                "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload"

#define MIN_PASSWORD_QUALITY_LEVEL            0.6

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
GlobalOptions          globalOptions;
String                 tmpDirectory;
DatabaseHandle         *indexDatabaseHandle;
Semaphore              inputLock;
Semaphore              outputLock;

LOCAL Commands         command;
LOCAL String           jobName;

LOCAL JobOptions       jobOptions;
LOCAL String           storageName;
//LOCAL FTPServerList    ftpServerList;
//LOCAL Semaphore        ftpServerListLock;
//LOCAL SSHServerList    sshServerList;
//LOCAL Semaphore        sshServerListLock;
//LOCAL WebDAVServerList webdavServerList;
//LOCAL Semaphore        webdavServerListLock;
LOCAL ServerList       serverList;
LOCAL Semaphore        serverListLock;
LOCAL DeviceList       deviceList;
LOCAL EntryList        includeEntryList;
LOCAL PatternList      excludePatternList;
LOCAL PatternList      deltaSourcePatternList;
LOCAL PatternList      compressExcludePatternList;
LOCAL ScheduleList     scheduleList;
LOCAL FTPServer        defaultFTPServer;
LOCAL ServerAllocation defaultFTPServerAllocation;
LOCAL SSHServer        defaultSSHServer;
LOCAL ServerAllocation defaultSSHServerAllocation;
LOCAL WebDAVServer     defaultWebDAVServer;
LOCAL ServerAllocation defaultWebDAVServerAllocation;
LOCAL Device           defaultDevice;
LOCAL FTPServer        *currentFTPServer;
LOCAL SSHServer        *currentSSHServer;
LOCAL WebDAVServer     *currentWebDAVServer;
LOCAL Device           *currentDevice;
LOCAL bool             daemonFlag;
LOCAL bool             noDetachFlag;
LOCAL uint             serverPort;
LOCAL uint             serverTLSPort;
LOCAL const char       *serverCAFileName;
LOCAL const char       *serverCertFileName;
LOCAL const char       *serverKeyFileName;
LOCAL Password         *serverPassword;
LOCAL const char       *serverJobsDirectory;

LOCAL const char       *indexDatabaseFileName;

LOCAL ulong            logTypes;
LOCAL const char       *logFileName;
LOCAL const char       *logPostCommand;

LOCAL bool             batchFlag;
LOCAL bool             versionFlag;
LOCAL bool             helpFlag,xhelpFlag,helpInternalFlag;

LOCAL const char       *pidFileName;

LOCAL String           keyFileName;
LOCAL uint             keyBits;

/*---------------------------------------------------------------------*/

LOCAL StringList         configFileNameList;  // list of configuration files to read

LOCAL String             tmpLogFileName;      // file name of temporary log file
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
LOCAL bool cmdOptionParseBandWidth(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseOwner(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);
LOCAL bool cmdOptionParseCompressAlgorithm(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize);

LOCAL const CommandLineUnit COMMAND_LINE_BYTES_UNITS[] =
{
  {"G",1024LL*1024LL*1024LL},
  {"M",1024LL*1024LL},
  {"K",1024LL},
};

LOCAL const CommandLineUnit COMMAND_LINE_BITS_UNITS[] =
{
  {"K",1024LL},
};

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_PATTERN_TYPES[] =
{
  {"glob",    PATTERN_TYPE_GLOB,          "glob patterns: * and ?"                      },
  {"regex",   PATTERN_TYPE_REGEX,         "regular expression pattern matching"         },
  {"extended",PATTERN_TYPE_EXTENDED_REGEX,"extended regular expression pattern matching"},
};

LOCAL const struct
{
  const char         *name;
  CompressAlgorithms compressAlgorithm;
} COMPRESS_ALGORITHMS_DELTA[] =
{
  {"none", COMPRESS_ALGORITHM_NONE},

  #ifdef HAVE_XDELTA
    {"xdelta1",COMPRESS_ALGORITHM_XDELTA_1},
    {"xdelta2",COMPRESS_ALGORITHM_XDELTA_2},
    {"xdelta3",COMPRESS_ALGORITHM_XDELTA_3},
    {"xdelta4",COMPRESS_ALGORITHM_XDELTA_4},
    {"xdelta5",COMPRESS_ALGORITHM_XDELTA_5},
    {"xdelta6",COMPRESS_ALGORITHM_XDELTA_6},
    {"xdelta7",COMPRESS_ALGORITHM_XDELTA_7},
    {"xdelta8",COMPRESS_ALGORITHM_XDELTA_8},
    {"xdelta9",COMPRESS_ALGORITHM_XDELTA_9},
  #endif /* HAVE_XDELTA */
};

LOCAL const struct
{
  const char         *name;
  CompressAlgorithms compressAlgorithm;
} COMPRESS_ALGORITHMS_BYTE[] =
{
  {"none",COMPRESS_ALGORITHM_NONE},

  {"zip0",COMPRESS_ALGORITHM_ZIP_0},
  {"zip1",COMPRESS_ALGORITHM_ZIP_1},
  {"zip2",COMPRESS_ALGORITHM_ZIP_2},
  {"zip3",COMPRESS_ALGORITHM_ZIP_3},
  {"zip4",COMPRESS_ALGORITHM_ZIP_4},
  {"zip5",COMPRESS_ALGORITHM_ZIP_5},
  {"zip6",COMPRESS_ALGORITHM_ZIP_6},
  {"zip7",COMPRESS_ALGORITHM_ZIP_7},
  {"zip8",COMPRESS_ALGORITHM_ZIP_8},
  {"zip9",COMPRESS_ALGORITHM_ZIP_9},

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
};

#if 0
const CommandLineOptionSelect COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHMS[] =
{
  {"none", COMPRESS_ALGORITHM_NONE,   "no compression"           },

  {"zip0", COMPRESS_ALGORITHM_ZIP_0,  "ZIP compression level 0"  },
  {"zip1", COMPRESS_ALGORITHM_ZIP_1,  "ZIP compression level 1"  },
  {"zip2", COMPRESS_ALGORITHM_ZIP_2,  "ZIP compression level 2"  },
  {"zip3", COMPRESS_ALGORITHM_ZIP_3,  "ZIP compression level 3"  },
  {"zip4", COMPRESS_ALGORITHM_ZIP_4,  "ZIP compression level 4"  },
  {"zip5", COMPRESS_ALGORITHM_ZIP_5,  "ZIP compression level 5"  },
  {"zip6", COMPRESS_ALGORITHM_ZIP_6,  "ZIP compression level 6"  },
  {"zip7", COMPRESS_ALGORITHM_ZIP_7,  "ZIP compression level 7"  },
  {"zip8", COMPRESS_ALGORITHM_ZIP_8,  "ZIP compression level 8"  },
  {"zip9", COMPRESS_ALGORITHM_ZIP_9,  "ZIP compression level 9"  },

  #ifdef HAVE_BZ2
    {"bzip1",COMPRESS_ALGORITHM_BZIP2_1,"BZIP2 compression level 1"},
    {"bzip2",COMPRESS_ALGORITHM_BZIP2_2,"BZIP2 compression level 2"},
    {"bzip3",COMPRESS_ALGORITHM_BZIP2_3,"BZIP2 compression level 3"},
    {"bzip4",COMPRESS_ALGORITHM_BZIP2_4,"BZIP2 compression level 4"},
    {"bzip5",COMPRESS_ALGORITHM_BZIP2_5,"BZIP2 compression level 5"},
    {"bzip6",COMPRESS_ALGORITHM_BZIP2_6,"BZIP2 compression level 6"},
    {"bzip7",COMPRESS_ALGORITHM_BZIP2_7,"BZIP2 compression level 7"},
    {"bzip8",COMPRESS_ALGORITHM_BZIP2_8,"BZIP2 compression level 8"},
    {"bzip9",COMPRESS_ALGORITHM_BZIP2_9,"BZIP2 compression level 9"},
  #endif /* HAVE_BZ2 */

  #ifdef HAVE_LZMA
    {"lzma1",COMPRESS_ALGORITHM_LZMA_1,"LZMA compression level 1"},
    {"lzma2",COMPRESS_ALGORITHM_LZMA_2,"LZMA compression level 2"},
    {"lzma3",COMPRESS_ALGORITHM_LZMA_3,"LZMA compression level 3"},
    {"lzma4",COMPRESS_ALGORITHM_LZMA_4,"LZMA compression level 4"},
    {"lzma5",COMPRESS_ALGORITHM_LZMA_5,"LZMA compression level 5"},
    {"lzma6",COMPRESS_ALGORITHM_LZMA_6,"LZMA compression level 6"},
    {"lzma7",COMPRESS_ALGORITHM_LZMA_7,"LZMA compression level 7"},
    {"lzma8",COMPRESS_ALGORITHM_LZMA_8,"LZMA compression level 8"},
    {"lzma9",COMPRESS_ALGORITHM_LZMA_9,"LZMA compression level 9"},
  #endif /* HAVE_LZMA */

#if 0
  #ifdef HAVE_XDELTA
    {"xdelta1",COMPRESS_ALGORITHM_XDELTA_1,"XDELTA compression level 1"},
    {"xdelta2",COMPRESS_ALGORITHM_XDELTA_2,"XDELTA compression level 2"},
    {"xdelta3",COMPRESS_ALGORITHM_XDELTA_3,"XDELTA compression level 3"},
    {"xdelta4",COMPRESS_ALGORITHM_XDELTA_4,"XDELTA compression level 4"},
    {"xdelta5",COMPRESS_ALGORITHM_XDELTA_5,"XDELTA compression level 5"},
    {"xdelta6",COMPRESS_ALGORITHM_XDELTA_6,"XDELTA compression level 6"},
    {"xdelta7",COMPRESS_ALGORITHM_XDELTA_7,"XDELTA compression level 7"},
    {"xdelta8",COMPRESS_ALGORITHM_XDELTA_8,"XDELTA compression level 8"},
    {"xdelta9",COMPRESS_ALGORITHM_XDELTA_9,"XDELTA compression level 9"},
  #endif /* HAVE_XDELTA */
#endif /* 0 */
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_DELTA_COMPRESS_ALGORITHMS[] =
{
  {"none", COMPRESS_ALGORITHM_NONE,   "no compression"           },

  #ifdef HAVE_XDELTA
    {"xdelta1",COMPRESS_ALGORITHM_XDELTA_1,"XDELTA compression level 1"},
    {"xdelta2",COMPRESS_ALGORITHM_XDELTA_2,"XDELTA compression level 2"},
    {"xdelta3",COMPRESS_ALGORITHM_XDELTA_3,"XDELTA compression level 3"},
    {"xdelta4",COMPRESS_ALGORITHM_XDELTA_4,"XDELTA compression level 4"},
    {"xdelta5",COMPRESS_ALGORITHM_XDELTA_5,"XDELTA compression level 5"},
    {"xdelta6",COMPRESS_ALGORITHM_XDELTA_6,"XDELTA compression level 6"},
    {"xdelta7",COMPRESS_ALGORITHM_XDELTA_7,"XDELTA compression level 7"},
    {"xdelta8",COMPRESS_ALGORITHM_XDELTA_8,"XDELTA compression level 8"},
    {"xdelta9",COMPRESS_ALGORITHM_XDELTA_9,"XDELTA compression level 9"},
  #endif /* HAVE_XDELTA */
};
#endif /* 0 */

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS[] =
{
  {"none",      CRYPT_ALGORITHM_NONE,      "no encryption"          },

  #ifdef HAVE_GCRYPT
    {"3DES",      CRYPT_ALGORITHM_3DES,      "3DES cipher"          },
    {"CAST5",     CRYPT_ALGORITHM_CAST5,     "CAST5 cipher"         },
    {"BLOWFISH",  CRYPT_ALGORITHM_BLOWFISH,  "Blowfish cipher"      },
    {"AES128",    CRYPT_ALGORITHM_AES128,    "AES cipher 128bit"    },
    {"AES192",    CRYPT_ALGORITHM_AES192,    "AES cipher 192bit"    },
    {"AES256",    CRYPT_ALGORITHM_AES256,    "AES cipher 256bit"    },
    {"TWOFISH128",CRYPT_ALGORITHM_TWOFISH128,"Twofish cipher 128bit"},
    {"TWOFISH256",CRYPT_ALGORITHM_TWOFISH256,"Twofish cipher 256bit"},
    {"SERPENT128",CRYPT_ALGORITHM_SERPENT128,"Serpent cipher 128bit"},
    {"SERPENT192",CRYPT_ALGORITHM_SERPENT192,"Serpent cipher 192bit"},
    {"SERPENT256",CRYPT_ALGORITHM_SERPENT256,"Serpent cipher 256bit"},
    {"CAMELLIA128",CRYPT_ALGORITHM_CAMELLIA128,"Camellia cipher 128bit"},
    {"CAMELLIA192",CRYPT_ALGORITHM_CAMELLIA192,"Camellia cipher 192bit"},
    {"CAMELLIA256",CRYPT_ALGORITHM_CAMELLIA256,"Camellia cipher 256bit"},
  #endif /* HAVE_GCRYPT */
};

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_CRYPT_TYPES[] =
{
  #ifdef HAVE_GCRYPT
    {"symmetric", CRYPT_TYPE_SYMMETRIC, "symmetric"},
    {"asymmetric",CRYPT_TYPE_ASYMMETRIC,"asymmetric"},
  #endif /* HAVE_GCRYPT */
};

LOCAL const ConfigValueSelect CONFIG_VALUE_PASSWORD_MODES[] =
{
  {"default",PASSWORD_MODE_DEFAULT,},
  {"ask",    PASSWORD_MODE_ASK,    },
  {"config", PASSWORD_MODE_CONFIG, },
};

LOCAL const CommandLineUnit COMMAND_LINE_TIME_UNITS[] =
{
  {"weeks",7*24*60*60},
  {"days",24*60*60},
  {"h",60*60},
  {"m",60},
  {"s",1},
};

LOCAL const CommandLineOptionSet COMMAND_LINE_OPTIONS_LOG_TYPES[] =
{
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
};

LOCAL CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM         ("create",                       'c',0,0,command,                                     COMMAND_CREATE_FILES,                                  "create new files archive"                                                 ),
  CMD_OPTION_ENUM         ("image",                        'm',0,0,command,                                     COMMAND_CREATE_IMAGES,                                 "create new images archive"                                                ),
  CMD_OPTION_ENUM         ("list",                         'l',0,0,command,                                     COMMAND_LIST,                                          "list contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("test",                         't',0,0,command,                                     COMMAND_TEST,                                          "test contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("compare",                      'd',0,0,command,                                     COMMAND_COMPARE,                                       "compare contents of archive with files and images"                        ),
  CMD_OPTION_ENUM         ("extract",                      'x',0,0,command,                                     COMMAND_RESTORE,                                       "restore archive"                                                          ),
  CMD_OPTION_ENUM         ("generate-keys",                0,  0,0,command,                                     COMMAND_GENERATE_KEYS,                                 "generate new public/private key pair"                                     ),
//  CMD_OPTION_ENUM         ("new-key-password",             0,  0,0,command,                                     COMMAND_NEW_KEY_PASSWORD,                            "set new private key password"                                             ),
  CMD_OPTION_INTEGER      ("generate-keys-bits",           0,  1,0,keyBits,                                     MIN_ASYMMETRIC_CRYPT_KEY_BITS,
                                                                                                                MAX_ASYMMETRIC_CRYPT_KEY_BITS,COMMAND_LINE_BITS_UNITS,"key bits"                                                                  ),
  CMD_OPTION_STRING       ("job",                          0,  0,0,jobName,                                                                                            "execute job","name"                                                       ),

  CMD_OPTION_ENUM         ("normal",                       0,  1,1,jobOptions.archiveType,                      ARCHIVE_TYPE_NORMAL,                                   "create normal archive (no incremental list file)"                         ),
  CMD_OPTION_ENUM         ("full",                         'f',0,1,jobOptions.archiveType,                      ARCHIVE_TYPE_FULL,                                     "create full archive and incremental list file"                            ),
  CMD_OPTION_ENUM         ("incremental",                  'i',0,1,jobOptions.archiveType,                      ARCHIVE_TYPE_INCREMENTAL,                              "create incremental archive"                                               ),
  CMD_OPTION_SPECIAL      ("incremental-list-file",        'I',1,1,&jobOptions.incrementalListFileName,         cmdOptionParseString,NULL,                             "incremental list file name (default: <archive name>.bid)","file name"     ),
  CMD_OPTION_ENUM         ("differential",                 0,  1,1,jobOptions.archiveType,                      ARCHIVE_TYPE_DIFFERENTIAL,                             "create differential archive"                                              ),

  CMD_OPTION_SELECT       ("pattern-type",                 0,  1,1,jobOptions.patternType,                      COMMAND_LINE_OPTIONS_PATTERN_TYPES,                    "select pattern type"                                                      ),

  CMD_OPTION_SPECIAL      ("include",                      '#',0,2,&includeEntryList,                           cmdOptionParseEntryPattern,NULL,                       "include pattern","pattern"                                                ),
  CMD_OPTION_SPECIAL      ("exclude",                      '!',0,2,&excludePatternList,                         cmdOptionParsePattern,NULL,                            "exclude pattern","pattern"                                                ),

  CMD_OPTION_SPECIAL      ("delta-source",                 0,  0,2,&deltaSourcePatternList,                     cmdOptionParsePattern,NULL,                            "source pattern","pattern"                                                 ),

  CMD_OPTION_SPECIAL      ("config",                       0,  1,0,NULL,                                        cmdOptionParseConfigFile,NULL,                         "configuration file","file name"                                           ),

  CMD_OPTION_STRING       ("tmp-directory",                0,  1,0,globalOptions.tmpDirectory,                                                                         "temporary directory","path"                                               ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",                 0,  1,0,globalOptions.maxTmpSize,                    0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,              "max. size of temporary files"                                             ),

  CMD_OPTION_INTEGER64    ("archive-part-size",            's',0,1,jobOptions.archivePartSize,                  0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,              "approximated archive part size"                                           ),

  CMD_OPTION_INTEGER      ("directory-strip",              'p',1,1,jobOptions.directoryStripCount,              0,MAX_INT,NULL,                                         "number of directories to strip on extract"                               ),
  CMD_OPTION_STRING       ("destination",                  0,  0,1,jobOptions.destination,                                                                            "destination to restore files/images","path"                                ),
  CMD_OPTION_SPECIAL      ("owner",                        0,  0,1,&jobOptions.owner,                           cmdOptionParseOwner,NULL,                              "user and group of restored files","user:group"                            ),

  CMD_OPTION_SPECIAL      ("compress-algorithm",           'z',0,1,&jobOptions.compressAlgorithm,               cmdOptionParseCompressAlgorithm,NULL,                  "select compress algorithms to use\n"
                                                                                                                                                                       "  none        : no compression (default)\n"
                                                                                                                                                                       "  zip0..zip9  : ZIP compression level 0..9"
                                                                                                                                                                       #ifdef HAVE_BZ2
                                                                                                                                                                       "\n"
                                                                                                                                                                       "  bzip1..bzip9: BZIP2 compression level 1..9"
                                                                                                                                                                       #endif
                                                                                                                                                                       #ifdef HAVE_LZMA
                                                                                                                                                                       "\n"
                                                                                                                                                                       "  lzma1..lzma9: LZMA compression level 1..9"
                                                                                                                                                                       #endif
                                                                                                                                                                       #ifdef HAVE_XDELTA
                                                                                                                                                                       "\n"
                                                                                                                                                                       "additional select with '+':\n"
                                                                                                                                                                       "  xdelta1..xdelta9: XDELTA compression level 1..9"
                                                                                                                                                                       #endif
                                                                                                                                                                       ,
                                                                                                                                                                       "algorithm|xdelta+algorithm"                                               ),
  CMD_OPTION_INTEGER      ("compress-min-size",            0,  1,1,globalOptions.compressMinFileSize,           0,MAX_INT,COMMAND_LINE_BYTES_UNITS,                    "minimal size of file for compression"                                     ),
  CMD_OPTION_SPECIAL      ("compress-exclude",             0,  0,2,&compressExcludePatternList,                 cmdOptionParsePattern,NULL,                            "exclude compression pattern","pattern"                                    ),

  CMD_OPTION_SELECT       ("crypt-algorithm",              'y',0,1,jobOptions.cryptAlgorithm,                   COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS,                 "select crypt algorithm to use"                                            ),
  CMD_OPTION_SELECT       ("crypt-type",                   0,  0,1,jobOptions.cryptType,                        COMMAND_LINE_OPTIONS_CRYPT_TYPES,                      "select crypt type"                                                        ),
  CMD_OPTION_SPECIAL      ("crypt-password",               0,  0,1,&globalOptions.cryptPassword,                cmdOptionParsePassword,NULL,                           "crypt password (use with care!)","password"                               ),
  CMD_OPTION_STRING       ("crypt-public-key",             0,  0,1,jobOptions.cryptPublicKeyFileName,                                                                  "public key for encryption","file name"                                    ),
  CMD_OPTION_STRING       ("crypt-private-key",            0,  0,1,jobOptions.cryptPrivateKeyFileName,                                                                 "private key for decryption","file name"                                   ),

  CMD_OPTION_STRING       ("ftp-login-name",               0,  0,1,defaultFTPServer.loginName,                                                                         "ftp login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ftp-password",                 0,  0,1,&defaultFTPServer.password,                  cmdOptionParsePassword,NULL,                           "ftp password (use with care!)","password"                                 ),
  CMD_OPTION_INTEGER      ("ftp-max-connections",          0,  0,1,defaultFTPServer.maxConnectionCount,         MAX_CONNECTION_COUNT_UNLIMITED,MAX_INT,NULL,           "max. number of concurrent ftp connections"                                ),

  CMD_OPTION_INTEGER      ("ssh-port",                     0,  0,1,defaultSSHServer.port,                       0,65535,NULL,                                          "ssh port"                                                                 ),
  CMD_OPTION_STRING       ("ssh-login-name",               0,  0,1,defaultSSHServer.loginName,                                                                         "ssh login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ssh-password",                 0,  0,1,&defaultSSHServer.password,                  cmdOptionParsePassword,NULL,                           "ssh password (use with care!)","password"                                 ),
  CMD_OPTION_STRING       ("ssh-public-key",               0,  1,1,defaultSSHServer.publicKeyFileName,                                                                 "ssh public key file name","file name"                                     ),
  CMD_OPTION_STRING       ("ssh-private-key",              0,  1,1,defaultSSHServer.privateKeyFileName,                                                                "ssh private key file name","file name"                                    ),
  CMD_OPTION_INTEGER      ("ssh-max-connections",          0,  0,1,defaultSSHServer.maxConnectionCount,         MAX_CONNECTION_COUNT_UNLIMITED,MAX_INT,NULL,           "max. number of concurrent ssh connections"                                ),

//  CMD_OPTION_INTEGER      ("webdav-port",                  0,  0,1,defaultWebDAVServer.port,                    0,65535,NULL,                                          "WebDAV port"                                                              ),
  CMD_OPTION_STRING       ("webdav-login-name",            0,  0,1,defaultWebDAVServer.loginName,                                                                      "WebDAV login name","name"                                                 ),
  CMD_OPTION_SPECIAL      ("webdav-password",              0,  0,1,&defaultWebDAVServer.password,               cmdOptionParsePassword,NULL,                           "WebDAV password (use with care!)","password"                              ),
  CMD_OPTION_INTEGER      ("webdav-max-connections",       0,  0,1,defaultWebDAVServer.maxConnectionCount,      MAX_CONNECTION_COUNT_UNLIMITED,MAX_INT,NULL,           "max. number of concurrent WebDAV connections"                             ),

  CMD_OPTION_BOOLEAN      ("daemon",                       0,  1,0,daemonFlag,                                                                                         "run in daemon mode"                                                       ),
  CMD_OPTION_BOOLEAN      ("no-detach",                    'D',1,0,noDetachFlag,                                                                                       "do not detach in daemon mode"                                             ),
  CMD_OPTION_INTEGER      ("server-port",                  0,  1,0,serverPort,                                  0,65535,NULL,                                          "server port"                                                              ),
  CMD_OPTION_INTEGER      ("server-tls-port",              0,  1,0,serverTLSPort,                               0,65535,NULL,                                          "TLS (SSL) server port"                                                    ),
  CMD_OPTION_CSTRING      ("server-ca-file",               0,  1,0,serverCAFileName,                                                                                   "TLS (SSL) server certificate authority file (CA file)","file name"        ),
  CMD_OPTION_CSTRING      ("server-cert-file",             0,  1,0,serverCertFileName,                                                                                 "TLS (SSL) server certificate file","file name"                            ),
  CMD_OPTION_CSTRING      ("server-key-file",              0,  1,0,serverKeyFileName,                                                                                  "TLS (SSL) server key file","file name"                                    ),
  CMD_OPTION_SPECIAL      ("server-password",              0,  1,0,&serverPassword,                             cmdOptionParsePassword,NULL,                           "server password (use with care!)","password"                              ),
  CMD_OPTION_CSTRING      ("server-jobs-directory",        0,  1,0,serverJobsDirectory,                                                                                "server job directory","path name"                                         ),

  CMD_OPTION_INTEGER      ("nice-level",                   0,  1,0,globalOptions.niceLevel,                     0,19,NULL,                                             "general nice level of processes/threads"                                  ),
  CMD_OPTION_INTEGER      ("max-threads",                  0,  1,0,globalOptions.maxThreads,                    0,65535,NULL,                                          "max. number of concurrent compress/encryption threads"                    ),

  CMD_OPTION_SPECIAL      ("max-band-width",               0,  1,0,&globalOptions.maxBandWidthList,             cmdOptionParseBandWidth,NULL,                          "max. network band width to use [bits/s]","number or file name"            ),

  CMD_OPTION_BOOLEAN      ("batch",                        0,  2,0,batchFlag,                                                                                          "run in batch mode"                                                        ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",        0,  1,0,&globalOptions.remoteBARExecutable,          cmdOptionParseString,NULL,                             "remote BAR executable","file name"                                        ),

  CMD_OPTION_STRING       ("file-write-pre-command",       0,  1,0,globalOptions.file.writePreProcessCommand,                                                          "write file pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("file-write-post-command",      0,  1,0,globalOptions.file.writePostProcessCommand,                                                         "write file post-process command","command"                                ),

  CMD_OPTION_STRING       ("ftp-write-pre-command",        0,  1,0,globalOptions.ftp.writePreProcessCommand,                                                           "write FTP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("ftp-write-post-command",       0,  1,0,globalOptions.ftp.writePostProcessCommand,                                                          "write FTP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("scp-write-pre-command",        0,  1,0,globalOptions.scp.writePreProcessCommand,                                                           "write SCP pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("scp-write-post-command",       0,  1,0,globalOptions.scp.writePostProcessCommand,                                                          "write SCP post-process command","command"                                 ),

  CMD_OPTION_STRING       ("sftp-write-pre-command",       0,  1,0,globalOptions.sftp.writePreProcessCommand,                                                          "write SFTP pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("sftp-write-post-command",      0,  1,0,globalOptions.sftp.writePostProcessCommand,                                                         "write SFTP post-process command","command"                                ),

  CMD_OPTION_STRING       ("webdav-write-pre-command",     0,  1,0,globalOptions.webdav.writePreProcessCommand,                                                        "write WebDAV pre-process command","command"                               ),
  CMD_OPTION_STRING       ("webdav-write-post-command",    0,  1,0,globalOptions.webdav.writePostProcessCommand,                                                       "write WebDAV post-process command","command"                              ),

  CMD_OPTION_STRING       ("cd-device",                    0,  1,0,globalOptions.cd.defaultDeviceName,                                                                 "default CD device","device name"                                          ),
  CMD_OPTION_STRING       ("cd-request-volume-command",    0,  1,0,globalOptions.cd.requestVolumeCommand,                                                              "request new CD volume command","command"                                  ),
  CMD_OPTION_STRING       ("cd-unload-volume-command",     0,  1,0,globalOptions.cd.unloadVolumeCommand,                                                               "unload CD volume command","command"                                       ),
  CMD_OPTION_STRING       ("cd-load-volume-command",       0,  1,0,globalOptions.cd.loadVolumeCommand,                                                                 "load CD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("cd-volume-size",               0,  1,0,globalOptions.cd.volumeSize,                 0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,            "CD volume size"                                                           ),
  CMD_OPTION_STRING       ("cd-image-pre-command",         0,  1,0,globalOptions.cd.imagePreProcessCommand,                                                            "make CD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("cd-image-post-command",        0,  1,0,globalOptions.cd.imagePostProcessCommand,                                                           "make CD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("cd-image-command",             0,  1,0,globalOptions.cd.imageCommand,                                                                      "make CD image command","command"                                          ),
  CMD_OPTION_STRING       ("cd-ecc-pre-command",           0,  1,0,globalOptions.cd.eccPreProcessCommand,                                                              "make CD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("cd-ecc-post-command",          0,  1,0,globalOptions.cd.eccPostProcessCommand,                                                             "make CD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("cd-ecc-command",               0,  1,0,globalOptions.cd.eccCommand,                                                                        "make CD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("cd-write-pre-command",         0,  1,0,globalOptions.cd.writePreProcessCommand,                                                            "write CD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("cd-write-post-command",        0,  1,0,globalOptions.cd.writePostProcessCommand,                                                           "write CD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("cd-write-command",             0,  1,0,globalOptions.cd.writeCommand,                                                                      "write CD command","command"                                               ),
  CMD_OPTION_STRING       ("cd-write-image-command",       0,  1,0,globalOptions.cd.writeImageCommand,                                                                 "write CD image command","command"                                         ),

  CMD_OPTION_STRING       ("dvd-device",                   0,  1,0,globalOptions.dvd.defaultDeviceName,                                                                "default DVD device","device name"                                         ),
  CMD_OPTION_STRING       ("dvd-request-volume-command",   0,  1,0,globalOptions.dvd.requestVolumeCommand,                                                             "request new DVD volume command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-unload-volume-command",    0,  1,0,globalOptions.dvd.unloadVolumeCommand,                                                              "unload DVD volume command","command"                                      ),
  CMD_OPTION_STRING       ("dvd-load-volume-command",      0,  1,0,globalOptions.dvd.loadVolumeCommand,                                                                "load DVD volume command","command"                                        ),
  CMD_OPTION_INTEGER64    ("dvd-volume-size",              0,  1,0,globalOptions.dvd.volumeSize,                0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,            "DVD volume size"                                                          ),
  CMD_OPTION_STRING       ("dvd-image-pre-command",        0,  1,0,globalOptions.dvd.imagePreProcessCommand,                                                           "make DVD image pre-process command","command"                             ),
  CMD_OPTION_STRING       ("dvd-image-post-command",       0,  1,0,globalOptions.dvd.imagePostProcessCommand,                                                          "make DVD image post-process command","command"                            ),
  CMD_OPTION_STRING       ("dvd-image-command",            0,  1,0,globalOptions.dvd.imageCommand,                                                                     "make DVD image command","command"                                         ),
  CMD_OPTION_STRING       ("dvd-ecc-pre-command",          0,  1,0,globalOptions.dvd.eccPreProcessCommand,                                                             "make DVD error-correction codes pre-process command","command"            ),
  CMD_OPTION_STRING       ("dvd-ecc-post-command",         0,  1,0,globalOptions.dvd.eccPostProcessCommand,                                                            "make DVD error-correction codes post-process command","command"           ),
  CMD_OPTION_STRING       ("dvd-ecc-command",              0,  1,0,globalOptions.dvd.eccCommand,                                                                       "make DVD error-correction codes command","command"                        ),
  CMD_OPTION_STRING       ("dvd-write-pre-command",        0,  1,0,globalOptions.dvd.writePreProcessCommand,                                                           "write DVD pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("dvd-write-post-command",       0,  1,0,globalOptions.dvd.writePostProcessCommand,                                                          "write DVD post-process command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-write-command",            0,  1,0,globalOptions.dvd.writeCommand,                                                                     "write DVD command","command"                                              ),
  CMD_OPTION_STRING       ("dvd-write-image-command",      0,  1,0,globalOptions.dvd.writeImageCommand,                                                                "write DVD image command","command"                                        ),

  CMD_OPTION_STRING       ("bd-device",                    0,  1,0,globalOptions.bd.defaultDeviceName,                                                                 "default BD device","device name"                                          ),
  CMD_OPTION_STRING       ("bd-request-volume-command",    0,  1,0,globalOptions.bd.requestVolumeCommand,                                                              "request new BD volume command","command"                                  ),
  CMD_OPTION_STRING       ("bd-unload-volume-command",     0,  1,0,globalOptions.bd.unloadVolumeCommand,                                                               "unload BD volume command","command"                                       ),
  CMD_OPTION_STRING       ("bd-load-volume-command",       0,  1,0,globalOptions.bd.loadVolumeCommand,                                                                 "load BD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("bd-volume-size",               0,  1,0,globalOptions.bd.volumeSize,                 0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,            "BD volume size"                                                           ),
  CMD_OPTION_STRING       ("bd-image-pre-command",         0,  1,0,globalOptions.bd.imagePreProcessCommand,                                                            "make BD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("bd-image-post-command",        0,  1,0,globalOptions.bd.imagePostProcessCommand,                                                           "make BD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("bd-image-command",             0,  1,0,globalOptions.bd.imageCommand,                                                                      "make BD image command","command"                                          ),
  CMD_OPTION_STRING       ("bd-ecc-pre-command",           0,  1,0,globalOptions.bd.eccPreProcessCommand,                                                              "make BD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("bd-ecc-post-command",          0,  1,0,globalOptions.bd.eccPostProcessCommand,                                                             "make BD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("bd-ecc-command",               0,  1,0,globalOptions.bd.eccCommand,                                                                        "make BD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("bd-write-pre-command",         0,  1,0,globalOptions.bd.writePreProcessCommand,                                                            "write BD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("bd-write-post-command",        0,  1,0,globalOptions.bd.writePostProcessCommand,                                                           "write BD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("bd-write-command",             0,  1,0,globalOptions.bd.writeCommand,                                                                      "write BD command","command"                                               ),
  CMD_OPTION_STRING       ("bd-write-image-command",       0,  1,0,globalOptions.bd.writeImageCommand,                                                                 "write BD image command","command"                                         ),

  CMD_OPTION_STRING       ("device",                       0,  1,0,defaultDevice.defaultDeviceName,                                                                    "default device","device name"                                             ),
  CMD_OPTION_STRING       ("device-request-volume-command",0,  1,0,defaultDevice.requestVolumeCommand,                                                                 "request new volume command","command"                                     ),
  CMD_OPTION_STRING       ("device-load-volume-command",   0,  1,0,defaultDevice.loadVolumeCommand,                                                                    "load volume command","command"                                            ),
  CMD_OPTION_STRING       ("device-unload-volume-command", 0,  1,0,defaultDevice.unloadVolumeCommand,                                                                  "unload volume command","command"                                          ),
  CMD_OPTION_INTEGER64    ("device-volume-size",           0,  1,0,defaultDevice.volumeSize,                    0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,            "volume size"                                                              ),
  CMD_OPTION_STRING       ("device-image-pre-command",     0,  1,0,defaultDevice.imagePreProcessCommand,                                                               "make image pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("device-image-post-command",    0,  1,0,defaultDevice.imagePostProcessCommand,                                                              "make image post-process command","command"                                ),
  CMD_OPTION_STRING       ("device-image-command",         0,  1,0,defaultDevice.imageCommand,                                                                         "make image command","command"                                             ),
  CMD_OPTION_STRING       ("device-ecc-pre-command",       0,  1,0,defaultDevice.eccPreProcessCommand,                                                                 "make error-correction codes pre-process command","command"                ),
  CMD_OPTION_STRING       ("device-ecc-post-command",      0,  1,0,defaultDevice.eccPostProcessCommand,                                                                "make error-correction codes post-process command","command"               ),
  CMD_OPTION_STRING       ("device-ecc-command",           0,  1,0,defaultDevice.eccCommand,                                                                           "make error-correction codes command","command"                            ),
  CMD_OPTION_STRING       ("device-write-pre-command",     0,  1,0,defaultDevice.writePreProcessCommand,                                                               "write device pre-process command","command"                               ),
  CMD_OPTION_STRING       ("device-write-post-command",    0,  1,0,defaultDevice.writePostProcessCommand,                                                              "write device post-process command","command"                              ),
  CMD_OPTION_STRING       ("device-write-command",         0,  1,0,defaultDevice.writeCommand,                                                                         "write device command","command"                                           ),

  CMD_OPTION_INTEGER64    ("volume-size",                  0,  1,1,jobOptions.volumeSize,                       0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,            "volume size"                                                              ),
  CMD_OPTION_BOOLEAN      ("ecc",                          0,  1,1,jobOptions.errorCorrectionCodesFlag,                                                                "add error-correction codes with 'dvdisaster' tool"                        ),
  CMD_OPTION_BOOLEAN      ("always-create-image",          0,  1,1,jobOptions.alwaysCreateImageFlag,                                                                   "always create image for CD/DVD/BD/device"                                 ),

  CMD_OPTION_CSTRING      ("index-database",               0,  1,0,indexDatabaseFileName,                                                                              "index database file name","file name"                                     ),
  CMD_OPTION_BOOLEAN      ("index-database-auto-update",   0,  1,0,globalOptions.indexDatabaseAutoUpdateFlag,                                                          "enabled automatic update index database"                                  ),
  CMD_OPTION_SPECIAL      ("index-database-max-band-width",0,  1,0,&globalOptions.indexDatabaseMaxBandWidthList,cmdOptionParseBandWidth,NULL,                          "max. band width to use for index updates [bis/s]","number or file name"   ),
  CMD_OPTION_INTEGER      ("index-database-keep-time",     0,  1,0,globalOptions.indexDatabaseKeepTime,         0,MAX_INT,COMMAND_LINE_TIME_UNITS,                     "time to keep index data of not existing storage"                          ),

  CMD_OPTION_SET          ("log",                          0,  1,0,logTypes,                                    COMMAND_LINE_OPTIONS_LOG_TYPES,                        "log types"                                                                ),
  CMD_OPTION_CSTRING      ("log-file",                     0,  1,0,logFileName,                                                                                        "log file name","file name"                                                ),
  CMD_OPTION_CSTRING      ("log-post-command",             0,  1,0,logPostCommand,                                                                                     "log file post-process command","command"                                  ),

  CMD_OPTION_CSTRING      ("pid-file",                     0,  1,0,pidFileName,                                                                                        "process id file name","file name"                                         ),

  CMD_OPTION_BOOLEAN      ("group",                        'g',0,0,globalOptions.groupFlag,                                                                            "group files in list"                                                      ),
  CMD_OPTION_BOOLEAN      ("all",                          0,  0,0,globalOptions.allFlag,                                                                              "show all files"                                                           ),
  CMD_OPTION_BOOLEAN      ("long-format",                  'L',0,0,globalOptions.longFormatFlag,                                                                       "list in long format"                                                      ),
  CMD_OPTION_BOOLEAN      ("human-format",                 'H',0,0,globalOptions.humanFormatFlag,                                                                      "list in human readable format"                                            ),
  CMD_OPTION_BOOLEAN      ("no-header-footer",             0,  0,0,globalOptions.noHeaderFooterFlag,                                                                   "output no header/footer in list"                                          ),
  CMD_OPTION_BOOLEAN      ("delete-old-archive-files",     0,  1,0,globalOptions.deleteOldArchiveFilesFlag,                                                            "delete old archive files after creating new files"                        ),
  CMD_OPTION_BOOLEAN      ("ignore-no-backup-file",        0,  1,1,globalOptions.ignoreNoBackupFileFlag,                                                               "ignore .nobackup/.NOBACKUP file"                                          ),
  CMD_OPTION_BOOLEAN      ("ignore-no-dump",               0,  1,1,jobOptions.ignoreNoDumpAttributeFlag,                                                               "ignore 'no dump' attribute of files"                                      ),

  CMD_OPTION_BOOLEAN      ("skip-unreadable",              0,  0,1,jobOptions.skipUnreadableFlag,                                                                      "skip unreadable files"                                                    ),
  CMD_OPTION_BOOLEAN      ("force-delta-compression",      0,  0,1,jobOptions.forceDeltaCompressionFlag,                                                               "force delta compression of files. Stop on error"                          ),
  CMD_OPTION_BOOLEAN      ("raw-images",                   0,  1,1,jobOptions.rawImagesFlag,                                                                           "store raw images (store all image blocks)"                                ),
  CMD_OPTION_BOOLEAN      ("no-fragments-check",           0,  1,1,jobOptions.noFragmentsCheckFlag,                                                                    "do not check completeness of file fragments"                              ),
  CMD_OPTION_BOOLEAN      ("no-index-database",            0,  1,0,jobOptions.noIndexDatabaseFlag,                                                                     "do not store index database for archives"                                 ),
  CMD_OPTION_BOOLEAN      ("overwrite-archive-files",      'o',0,1,jobOptions.overwriteArchiveFilesFlag,                                                               "overwrite existing archive files"                                         ),
  CMD_OPTION_BOOLEAN      ("overwrite-files",              0,  0,1,jobOptions.overwriteFilesFlag,                                                                      "overwrite existing files"                                                 ),
  CMD_OPTION_BOOLEAN      ("wait-first-volume",            0,  1,1,jobOptions.waitFirstVolumeFlag,                                                                     "wait for first volume"                                                    ),
  CMD_OPTION_BOOLEAN      ("dry-run",                      0,  1,1,jobOptions.dryRunFlag,                                                                              "do dry-run (skip storage/restore, incremental data, index database)"      ),
  CMD_OPTION_BOOLEAN      ("no-storage",                   0,  1,1,jobOptions.noStorageFlag,                                                                           "do not store archives (skip storage, index database)"                     ),
  CMD_OPTION_BOOLEAN      ("no-bar-on-medium",             0,  1,1,jobOptions.noBAROnMediumFlag,                                                                       "do not store a copy of BAR on medium"                                     ),
  CMD_OPTION_BOOLEAN      ("stop-on-error",                0,  1,1,jobOptions.stopOnErrorFlag,                                                                         "immediately stop on error"                                                ),

  CMD_OPTION_BOOLEAN      ("no-default-config",            0,  1,0,globalOptions.noDefaultConfigFlag,                                                                  "do not read personal configuration file ~/.bar/" DEFAULT_CONFIG_FILE_NAME ),
  CMD_OPTION_BOOLEAN      ("quiet",                        0,  1,0,globalOptions.quietFlag,                                                                            "suppress any output"                                                      ),
  CMD_OPTION_INTEGER_RANGE("verbose",                      'v',1,0,globalOptions.verboseLevel,                  0,6,NULL,                                              "verbosity level"                                                          ),

  CMD_OPTION_BOOLEAN      ("server-debug",                 0,  2,0,globalOptions.serverDebugFlag,                                                                      "enable debug mode for server"                                             ),

  CMD_OPTION_BOOLEAN      ("version",                      0  ,0,0,versionFlag,                                                                                        "output version"                                                           ),
  CMD_OPTION_BOOLEAN      ("help",                         'h',0,0,helpFlag,                                                                                           "output this help"                                                         ),
  CMD_OPTION_BOOLEAN      ("xhelp",                        0,  0,0,xhelpFlag,                                                                                          "output help to extended options"                                          ),
  CMD_OPTION_BOOLEAN      ("help-internal",                0,  1,0,helpInternalFlag,                                                                                   "output help to internal options"                                          ),
};

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

LOCAL const ConfigValueUnit CONFIG_VALUE_BYTES_UNITS[] =
{
  {"K",1024LL},
  {"M",1024LL*1024LL},
  {"G",1024LL*1024LL*1024LL},
  {"T",1024LL*1024LL*1024LL*1024LL},
};

LOCAL const ConfigValueUnit CONFIG_VALUE_BITS_UNITS[] =
{
  {"K",1024LL},
};

LOCAL const ConfigValueSelect CONFIG_VALUE_ARCHIVE_TYPES[] =
{
  {"normal",     ARCHIVE_TYPE_NORMAL,    },
  {"full",       ARCHIVE_TYPE_FULL,      },
  {"incremental",ARCHIVE_TYPE_INCREMENTAL},
};

LOCAL const ConfigValueSelect CONFIG_VALUE_PATTERN_TYPES[] =
{
  {"glob",    PATTERN_TYPE_GLOB,         },
  {"regex",   PATTERN_TYPE_REGEX,        },
  {"extended",PATTERN_TYPE_EXTENDED_REGEX},
};

LOCAL const ConfigValueSelect CONFIG_VALUE_COMPRESS_ALGORITHMS[] =
{
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
};

LOCAL const ConfigValueSelect CONFIG_VALUE_CRYPT_ALGORITHMS[] =
{
  {"none",      CRYPT_ALGORITHM_NONE,     },

  #ifdef HAVE_GCRYPT
    {"3DES",      CRYPT_ALGORITHM_3DES,     },
    {"CAST5",     CRYPT_ALGORITHM_CAST5,    },
    {"BLOWFISH",  CRYPT_ALGORITHM_BLOWFISH, },
    {"AES128",    CRYPT_ALGORITHM_AES128,   },
    {"AES192",    CRYPT_ALGORITHM_AES192,   },
    {"AES256",    CRYPT_ALGORITHM_AES256,   },
    {"TWOFISH128",CRYPT_ALGORITHM_TWOFISH128},
    {"TWOFISH256",CRYPT_ALGORITHM_TWOFISH256},
    {"SERPENT128",CRYPT_ALGORITHM_SERPENT128},
    {"SERPENT192",CRYPT_ALGORITHM_SERPENT192},
    {"SERPENT256",CRYPT_ALGORITHM_SERPENT256},
    {"CAMELLIA128",CRYPT_ALGORITHM_CAMELLIA128},
    {"CAMELLIA192",CRYPT_ALGORITHM_CAMELLIA192},
    {"CAMELLIA256",CRYPT_ALGORITHM_CAMELLIA256},
  #endif /* HAVE_GCRYPT */
};

LOCAL const ConfigValueSelect CONFIG_VALUE_CRYPT_TYPES[] =
{
  #ifdef HAVE_GCRYPT
    {"symmetric", CRYPT_TYPE_SYMMETRIC },
    {"asymmetric",CRYPT_TYPE_ASYMMETRIC},
  #endif /* HAVE_GCRYPT */
};

LOCAL const ConfigValueUnit CONFIG_VALUE_TIME_UNITS[] =
{
  {"weeks",7*24*60*60},
  {"days",24*60*60},
  {"h",60*60},
  {"m",60},
  {"s",1},
};

LOCAL const ConfigValueSet CONFIG_VALUE_LOG_TYPES[] =
{
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
};

LOCAL const ConfigValue CONFIG_VALUES[] =
{
  // general settings
  CONFIG_VALUE_SPECIAL  ("config",                       NULL,-1,                                                 configValueParseConfigFile,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_STRING   ("tmp-directory",                &globalOptions.tmpDirectory,-1                           ),
  CONFIG_VALUE_INTEGER64("max-tmp-size",                 &globalOptions.maxTmpSize,-1,                            0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER  ("nice-level",                   &globalOptions.niceLevel,-1,                             0,19,NULL),
  CONFIG_VALUE_INTEGER  ("max-threads",                  &globalOptions.maxThreads,-1,                            0,65535,NULL),

  CONFIG_VALUE_SPECIAL  ("max-band-width",               &globalOptions.maxBandWidthList,-1,                      configValueParseBandWidth,NULL,NULL,NULL,&globalOptions.maxBandWidthList),

  CONFIG_VALUE_INTEGER  ("compress-min-size",            &globalOptions.compressMinFileSize,-1,                   0,MAX_INT,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL  ("compress-exclude",             &compressExcludePatternList,-1,                          configValueParsePattern,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_CSTRING  ("index-database",               &indexDatabaseFileName,-1                                ),
  CONFIG_VALUE_BOOLEAN  ("index-database-auto-update",   &globalOptions.indexDatabaseAutoUpdateFlag,-1            ),
  CONFIG_VALUE_SPECIAL  ("index-database-max-band-width",&globalOptions.indexDatabaseMaxBandWidthList,-1,         configValueParseBandWidth,NULL,NULL,NULL,&globalOptions.indexDatabaseMaxBandWidthList),
  CONFIG_VALUE_INTEGER  ("index-database-keep-time",     &globalOptions.indexDatabaseKeepTime,-1,                 0,MAX_INT,CONFIG_VALUE_TIME_UNITS),

  // global job settings
  CONFIG_VALUE_STRING   ("archive-name",                 &storageName,-1                                          ),
  CONFIG_VALUE_SELECT   ("archive-type",                 &jobOptions.archiveType,-1,                              CONFIG_VALUE_ARCHIVE_TYPES),

  CONFIG_VALUE_STRING   ("incremental-list-file",        &jobOptions.incrementalListFileName,-1                   ),

  CONFIG_VALUE_INTEGER64("archive-part-size",            &jobOptions.archivePartSize,-1,                          0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER  ("directory-strip",              &jobOptions.directoryStripCount,-1,                      0,MAX_INT,NULL),
  CONFIG_VALUE_STRING   ("destination",                  &jobOptions.destination,-1                               ),
  CONFIG_VALUE_SPECIAL  ("owner",                        &jobOptions.owner,-1,                                    configValueParseOwner,NULL,NULL,NULL,&jobOptions),

  CONFIG_VALUE_SELECT   ("pattern-type",                 &jobOptions.patternType,-1,                              CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_VALUE_SPECIAL  ("compress-algorithm",           &jobOptions.compressAlgorithm,-1,                        configValueParseCompressAlgorithm,NULL,NULL,NULL,&jobOptions),

  CONFIG_VALUE_SELECT   ("crypt-algorithm",              &jobOptions.cryptAlgorithm,-1,                           CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_VALUE_SELECT   ("crypt-type",                   &jobOptions.cryptType,-1,                                CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_VALUE_SELECT   ("crypt-password-mode",          &jobOptions.cryptPasswordMode,-1,                        CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_VALUE_SPECIAL  ("crypt-password",               &globalOptions.cryptPassword,-1,                         configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_STRING   ("crypt-public-key",             &jobOptions.cryptPublicKeyFileName,-1                    ),
  CONFIG_VALUE_STRING   ("crypt-private-key",            &jobOptions.cryptPrivateKeyFileName,-1                   ),

  CONFIG_VALUE_STRING   ("ftp-login-name",               &currentFTPServer,offsetof(FTPServer,loginName)          ),
  CONFIG_VALUE_SPECIAL  ("ftp-password",                 &currentFTPServer,offsetof(FTPServer,password),          configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_INTEGER  ("ftp-max-connections",          &defaultFTPServer.maxConnectionCount,-1,                 -1,MAX_INT,NULL),

  CONFIG_VALUE_INTEGER  ("ssh-port",                     &currentSSHServer,offsetof(SSHServer,port),              0,65535,NULL),
  CONFIG_VALUE_STRING   ("ssh-login-name",               &currentSSHServer,offsetof(SSHServer,loginName)          ),
  CONFIG_VALUE_SPECIAL  ("ssh-password",                 &currentSSHServer,offsetof(SSHServer,password),          configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_STRING   ("ssh-public-key",               &currentSSHServer,offsetof(SSHServer,publicKeyFileName)  ),
  CONFIG_VALUE_STRING   ("ssh-private-key",              &currentSSHServer,offsetof(SSHServer,privateKeyFileName) ),
  CONFIG_VALUE_INTEGER  ("ssh-max-connections",          &defaultSSHServer.maxConnectionCount,-1,                 -1,MAX_INT,NULL),

//  CONFIG_VALUE_INTEGER  ("webdav-port",                  &currentWebDAVServer,offsetof(WebDAVServer,port),        0,65535,NULL),
  CONFIG_VALUE_STRING   ("webdav-login-name",            &currentWebDAVServer,offsetof(WebDAVServer,loginName)    ),
  CONFIG_VALUE_SPECIAL  ("webdav-password",              &currentWebDAVServer,offsetof(WebDAVServer,password),    configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_INTEGER  ("webdav-max-connections",       &defaultWebDAVServer.maxConnectionCount,-1,              -1,MAX_INT,NULL),

  CONFIG_VALUE_SPECIAL  ("include-file",                 &includeEntryList,-1,                                    configValueParseFileEntry,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("include-image",                &includeEntryList,-1,                                    configValueParseImageEntry,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("exclude",                      &excludePatternList,-1,                                  configValueParsePattern,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("exclude-compress",             &compressExcludePatternList,-1,                          configValueParsePattern,NULL,NULL,NULL,&jobOptions.patternType),

  CONFIG_VALUE_INTEGER64("volume-size",                  &jobOptions.volumeSize,-1,                               0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_BOOLEAN  ("ecc",                          &jobOptions.errorCorrectionCodesFlag,-1                  ),
  CONFIG_VALUE_BOOLEAN  ("always-create-image",          &jobOptions.alwaysCreateImageFlag,-1                     ),

  CONFIG_VALUE_BOOLEAN  ("skip-unreadable",              &jobOptions.skipUnreadableFlag,-1                        ),
  CONFIG_VALUE_BOOLEAN  ("raw-images",                   &jobOptions.rawImagesFlag,-1                             ),
  CONFIG_VALUE_BOOLEAN  ("no-fragments-check",           &jobOptions.noFragmentsCheckFlag,-1                      ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-archive-files",      &jobOptions.overwriteArchiveFilesFlag,-1                 ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-files",              &jobOptions.overwriteFilesFlag,-1                        ),
  CONFIG_VALUE_BOOLEAN  ("wait-first-volume",            &jobOptions.waitFirstVolumeFlag,-1                       ),
  CONFIG_VALUE_BOOLEAN  ("no-bar-on-medium",             &jobOptions.noBAROnMediumFlag,-1                         ),
  CONFIG_VALUE_BOOLEAN  ("quiet",                        &globalOptions.quietFlag,-1                              ),
  CONFIG_VALUE_INTEGER  ("verbose",                      &globalOptions.verboseLevel,-1,                          0,6,NULL),

  // igored job settings (server only)

  CONFIG_VALUE_SPECIAL  ("schedule",                     &scheduleList,-1,                                        configValueParseSchedule,configValueFormatInitSchedule,configValueFormatDoneSchedule,configValueFormatSchedule,NULL),

  // commands

  CONFIG_VALUE_STRING   ("file-write-pre-command",       &globalOptions.file.writePreProcessCommand,-1            ),
  CONFIG_VALUE_STRING   ("file-write-post-command",      &globalOptions.file.writePostProcessCommand,-1           ),

  CONFIG_VALUE_STRING   ("ftp-write-pre-command",        &globalOptions.ftp.writePreProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("ftp-write-post-command",       &globalOptions.ftp.writePostProcessCommand,-1            ),

  CONFIG_VALUE_STRING   ("scp-write-pre-command",        &globalOptions.scp.writePreProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("scp-write-post-command",       &globalOptions.scp.writePostProcessCommand,-1            ),

  CONFIG_VALUE_STRING   ("sftp-write-pre-command",       &globalOptions.sftp.writePreProcessCommand,-1            ),
  CONFIG_VALUE_STRING   ("sftp-write-post-command",      &globalOptions.sftp.writePostProcessCommand,-1           ),

  CONFIG_VALUE_STRING   ("webdav-write-pre-command",     &globalOptions.webdav.writePreProcessCommand,-1          ),
  CONFIG_VALUE_STRING   ("webdav-write-post-command",    &globalOptions.webdav.writePostProcessCommand,-1         ),

  CONFIG_VALUE_STRING   ("cd-device",                    &globalOptions.bd.defaultDeviceName,-1                   ),
  CONFIG_VALUE_STRING   ("cd-request-volume-command",    &globalOptions.cd.requestVolumeCommand,-1                ),
  CONFIG_VALUE_STRING   ("cd-unload-volume-command",     &globalOptions.cd.unloadVolumeCommand,-1                 ),
  CONFIG_VALUE_STRING   ("cd-load-volume-command",       &globalOptions.cd.loadVolumeCommand,-1                   ),
  CONFIG_VALUE_INTEGER64("cd-volume-size",               &globalOptions.cd.volumeSize,-1,                         0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("cd-image-pre-command",         &globalOptions.cd.imagePreProcessCommand,-1              ),
  CONFIG_VALUE_STRING   ("cd-image-post-command",        &globalOptions.cd.imagePostProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("cd-image-command",             &globalOptions.cd.imageCommand,-1                        ),
  CONFIG_VALUE_STRING   ("cd-ecc-pre-command",           &globalOptions.cd.eccPreProcessCommand,-1                ),
  CONFIG_VALUE_STRING   ("cd-ecc-post-command",          &globalOptions.cd.eccPostProcessCommand,-1               ),
  CONFIG_VALUE_STRING   ("cd-ecc-command",               &globalOptions.cd.eccCommand,-1                          ),
  CONFIG_VALUE_STRING   ("cd-write-pre-command",         &globalOptions.cd.writePreProcessCommand,-1              ),
  CONFIG_VALUE_STRING   ("cd-write-post-command",        &globalOptions.cd.writePostProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("cd-write-command",             &globalOptions.cd.writeCommand,-1                        ),
  CONFIG_VALUE_STRING   ("cd-write-image-command",       &globalOptions.cd.writeImageCommand,-1                   ),

  CONFIG_VALUE_STRING   ("dvd-device",                   &globalOptions.bd.defaultDeviceName,-1                   ),
  CONFIG_VALUE_STRING   ("dvd-request-volume-command",   &globalOptions.dvd.requestVolumeCommand,-1               ),
  CONFIG_VALUE_STRING   ("dvd-unload-volume-command",    &globalOptions.dvd.unloadVolumeCommand,-1                ),
  CONFIG_VALUE_STRING   ("dvd-load-volume-command",      &globalOptions.dvd.loadVolumeCommand,-1                  ),
  CONFIG_VALUE_INTEGER64("dvd-volume-size",              &globalOptions.dvd.volumeSize,-1,                        0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("dvd-image-pre-command",        &globalOptions.dvd.imagePreProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("dvd-image-post-command",       &globalOptions.dvd.imagePostProcessCommand,-1            ),
  CONFIG_VALUE_STRING   ("dvd-image-command",            &globalOptions.dvd.imageCommand,-1                       ),
  CONFIG_VALUE_STRING   ("dvd-ecc-pre-command",          &globalOptions.dvd.eccPreProcessCommand,-1               ),
  CONFIG_VALUE_STRING   ("dvd-ecc-post-command",         &globalOptions.dvd.eccPostProcessCommand,-1              ),
  CONFIG_VALUE_STRING   ("dvd-ecc-command",              &globalOptions.dvd.eccCommand,-1                         ),
  CONFIG_VALUE_STRING   ("dvd-write-pre-command",        &globalOptions.dvd.writePreProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("dvd-write-post-command",       &globalOptions.dvd.writePostProcessCommand,-1            ),
  CONFIG_VALUE_STRING   ("dvd-write-command",            &globalOptions.dvd.writeCommand,-1                       ),
  CONFIG_VALUE_STRING   ("dvd-write-image-command",      &globalOptions.dvd.writeImageCommand,-1                  ),

  CONFIG_VALUE_STRING   ("bd-device",                    &globalOptions.bd.defaultDeviceName,-1                   ),
  CONFIG_VALUE_STRING   ("bd-request-volume-command",    &globalOptions.bd.requestVolumeCommand,-1                ),
  CONFIG_VALUE_STRING   ("bd-unload-volume-command",     &globalOptions.bd.unloadVolumeCommand,-1                 ),
  CONFIG_VALUE_STRING   ("bd-load-volume-command",       &globalOptions.bd.loadVolumeCommand,-1                   ),
  CONFIG_VALUE_INTEGER64("bd-volume-size",               &globalOptions.bd.volumeSize,-1,                         0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("bd-image-pre-command",         &globalOptions.bd.imagePreProcessCommand,-1              ),
  CONFIG_VALUE_STRING   ("bd-image-post-command",        &globalOptions.bd.imagePostProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("bd-image-command",             &globalOptions.bd.imageCommand,-1                        ),
  CONFIG_VALUE_STRING   ("bd-ecc-pre-command",           &globalOptions.bd.eccPreProcessCommand,-1                ),
  CONFIG_VALUE_STRING   ("bd-ecc-post-command",          &globalOptions.bd.eccPostProcessCommand,-1               ),
  CONFIG_VALUE_STRING   ("bd-ecc-command",               &globalOptions.bd.eccCommand,-1                          ),
  CONFIG_VALUE_STRING   ("bd-write-pre-command",         &globalOptions.bd.writePreProcessCommand,-1              ),
  CONFIG_VALUE_STRING   ("bd-write-post-command",        &globalOptions.bd.writePostProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("bd-write-command",             &globalOptions.bd.writeCommand,-1                        ),
  CONFIG_VALUE_STRING   ("bd-write-image-command",       &globalOptions.bd.writeImageCommand,-1                   ),

  CONFIG_VALUE_STRING   ("device",                       &currentDevice,offsetof(Device,defaultDeviceName)        ),
  CONFIG_VALUE_STRING   ("device-request-volume-command",&currentDevice,offsetof(Device,requestVolumeCommand)     ),
  CONFIG_VALUE_STRING   ("device-unload-volume-command", &currentDevice,offsetof(Device,unloadVolumeCommand)      ),
  CONFIG_VALUE_STRING   ("device-load-volume-command",   &currentDevice,offsetof(Device,loadVolumeCommand)        ),
  CONFIG_VALUE_INTEGER64("device-volume-size",           &currentDevice,offsetof(Device,volumeSize),              0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("device-image-pre-command",     &currentDevice,offsetof(Device,imagePreProcessCommand)   ),
  CONFIG_VALUE_STRING   ("device-image-post-command",    &currentDevice,offsetof(Device,imagePostProcessCommand)  ),
  CONFIG_VALUE_STRING   ("device-image-command",         &currentDevice,offsetof(Device,imageCommand)             ),
  CONFIG_VALUE_STRING   ("device-ecc-pre-command",       &currentDevice,offsetof(Device,eccPreProcessCommand)     ),
  CONFIG_VALUE_STRING   ("device-ecc-post-command",      &currentDevice,offsetof(Device,eccPostProcessCommand)    ),
  CONFIG_VALUE_STRING   ("device-ecc-command",           &currentDevice,offsetof(Device,eccCommand)               ),
  CONFIG_VALUE_STRING   ("device-write-pre-command",     &currentDevice,offsetof(Device,writePreProcessCommand)   ),
  CONFIG_VALUE_STRING   ("device-write-post-command",    &currentDevice,offsetof(Device,writePostProcessCommand)  ),
  CONFIG_VALUE_STRING   ("device-write-command",         &currentDevice,offsetof(Device,writeCommand)             ),

  // server settings

  CONFIG_VALUE_INTEGER  ("server-port",                  &serverPort,-1,                                          0,65535,NULL),
  CONFIG_VALUE_INTEGER  ("server-tls-port",              &serverTLSPort,-1,                                       0,65535,NULL),
  CONFIG_VALUE_CSTRING  ("server-ca-file",               &serverCAFileName,-1                                     ),
  CONFIG_VALUE_CSTRING  ("server-cert-file",             &serverCertFileName,-1                                   ),
  CONFIG_VALUE_CSTRING  ("server-key-file",              &serverKeyFileName,-1                                    ),
  CONFIG_VALUE_SPECIAL  ("server-password",              &serverPassword,-1,                                      configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_CSTRING  ("server-jobs-directory",        &serverJobsDirectory,-1                                  ),

  CONFIG_VALUE_STRING   ("remote-bar-executable",        &globalOptions.remoteBARExecutable,-1                    ),

  CONFIG_VALUE_SET      ("log",                          &logTypes,-1,                                            CONFIG_VALUE_LOG_TYPES),
  CONFIG_VALUE_CSTRING  ("log-file",                     &logFileName,-1                                          ),
  CONFIG_VALUE_CSTRING  ("log-post-command",             &logPostCommand,-1                                       ),

  CONFIG_VALUE_CSTRING  ("pid-file",                     &pidFileName,-1                                          ),
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
* Name   : output
* Purpose: output string to console
* Input  : file   - output stream (stdout, stderr)
*          string - string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void output(FILE *file, const String string)
{
  SemaphoreLock semaphoreLock;
  String        outputLine;
  ulong         z;
  char          ch;

//fprintf(stderr,"%s, %d: string=%s\n",__FILE__,__LINE__,String_cString(string));
  outputLine = (String)Thread_getLocalVariable(&outputLineHandle);
  if (outputLine != NULL)
  {
    if (File_isTerminal(file))
    {
      // lock
      SEMAPHORE_LOCKED_DO(semaphoreLock,&outputLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        // restore output line
        if (outputLine != lastOutputLine)
        {
          // wipe-out current line
          for (z = 0; z < String_length(lastOutputLine); z++)
          {
            fwrite("\b",1,1,file);
          }
          for (z = 0; z < String_length(lastOutputLine); z++)
          {
            fwrite(" ",1,1,file);
          }
          for (z = 0; z < String_length(lastOutputLine); z++)
          {
            fwrite("\b",1,1,file);
          }

          // restore line
          fwrite(String_cString(outputLine),1,String_length(outputLine),file);
        }

        // output string
        fwrite(String_cString(string),1,String_length(string),file);

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
    }
    else
    {
      if (String_index(string,STRING_END) == '\n')
      {
        if (outputLine != NULL) fwrite(String_cString(outputLine),1,String_length(outputLine),file);
        fwrite(String_cString(string),1,String_length(string),file);
        String_clear(outputLine);
      }
      else
      {
        String_append(outputLine,string);
      }
    }
  }
  else
  {
    // no thread local vairable -> output string
    fwrite(String_cString(string),1,String_length(string),file);
  }
  fflush(file);
}

/***********************************************************************\
* Name   : initServerAllocate
* Purpose: init server allocation handle
* Input  : serverAllocation - server allocation handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initServerAllocate(ServerAllocation *serverAllocation)
{
  assert(serverAllocation != NULL);

  if (!Semaphore_init(&serverAllocation->lock))
  {
    HALT_FATAL_ERROR("cannot initialize semaphore");
  }
  serverAllocation->lowPriorityRequestCount  = 0;
  serverAllocation->highPriorityRequestCount = 0;
  serverAllocation->connectionCount          = 0;
}

/***********************************************************************\
* Name   : doneServerAllocate
* Purpose: done server allocation handle
* Input  : serverAllocation - server allocation handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneServerAllocate(ServerAllocation *serverAllocation)
{
  assert(serverAllocation != NULL);

  Semaphore_done(&serverAllocation->lock);
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

LOCAL bool readConfigFile(const String fileName, bool printInfoFlag)
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
  error = File_getFileInfo(&fileInfo,fileName);
  if (error == ERROR_NONE)
  {
    if ((fileInfo.permission & (FILE_PERMISSION_GROUP_READ|FILE_PERMISSION_OTHER_READ)) != 0)
    {
      printWarning("Configuration file '%s' has wrong file permission %03o. Please make sure read permissions are limited to file owner (mode 600).\n",
                   String_cString(fileName),
                   fileInfo.permission & FILE_PERMISSION_MASK
                  );
    }
  }
  else
  {
    printWarning("Cannot get file info for configuration file '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
  }

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open configuration file '%s' (error: %s)!\n",
               String_cString(fileName),
               Error_getText(error)
              );
    return FALSE;
  }

  // parse file
  if (isPrintInfo(2) || printInfoFlag) printf("Reading configuration file '%s'...",String_cString(fileName));
  failFlag   = FALSE;
  lineNb     = 0;
  line       = String_new();
  name       = String_new();
  value      = String_new();
  while (!File_eof(&fileHandle) && !failFlag)
  {
    // read line
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      if (printInfoFlag) printf("FAIL!\n");
      printError("Cannot read file '%s' (error: %s)!\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      failFlag = TRUE;
      break;
    }
    String_trim(line,STRING_WHITE_SPACES);
    lineNb++;

    // skip comments, empty lines
    if (String_isEmpty(line) || String_startsWithChar(line,'#'))
    {
      continue;
    }

    // parse line
    if      (String_parse(line,STRING_BEGIN,"[ftp-server %S]",NULL,name))
    {
      ServerNode *serverNode;

      serverNode = LIST_NEW_NODE(ServerNode);
      if (serverNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      serverNode->type                         = SERVER_TYPE_FTP;
      serverNode->name                         = String_duplicate(name);
      initServerAllocate(&serverNode->serverAllocation);
      serverNode->ftpServer.loginName          = NULL;
      serverNode->ftpServer.password           = NULL;
      serverNode->ftpServer.maxConnectionCount = MAX_CONNECTION_COUNT_UNLIMITED;

      List_append(&serverList,serverNode);

      currentFTPServer = &serverNode->ftpServer;
    }
    else if (String_parse(line,STRING_BEGIN,"[ssh-server %S]",NULL,name))
    {
      ServerNode *serverNode;

      serverNode = LIST_NEW_NODE(ServerNode);
      if (serverNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      serverNode->type                         = SERVER_TYPE_SSH;
      serverNode->name                         = String_duplicate(name);
      initServerAllocate(&serverNode->serverAllocation);
      serverNode->sshServer.port               = 22;
      serverNode->sshServer.loginName          = NULL;
      serverNode->sshServer.password           = NULL;
      serverNode->sshServer.publicKeyFileName  = NULL;
      serverNode->sshServer.privateKeyFileName = NULL;
      serverNode->sshServer.maxConnectionCount = MAX_CONNECTION_COUNT_UNLIMITED;

      List_append(&serverList,serverNode);

      currentSSHServer = &serverNode->sshServer;
    }
    else if (String_parse(line,STRING_BEGIN,"[webdav-server %S]",NULL,name))
    {
      ServerNode *serverNode;

      serverNode = LIST_NEW_NODE(ServerNode);
      if (serverNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      serverNode->type                            = SERVER_TYPE_WEBDAV;
      serverNode->name                            = String_duplicate(name);
      initServerAllocate(&serverNode->serverAllocation);
//      serverNode->webdavServer.port               = 80;
      serverNode->webdavServer.loginName          = NULL;
      serverNode->webdavServer.password           = NULL;
      serverNode->webdavServer.publicKeyFileName  = NULL;
      serverNode->webdavServer.privateKeyFileName = NULL;
      serverNode->webdavServer.maxConnectionCount = MAX_CONNECTION_COUNT_UNLIMITED;

      List_append(&serverList,serverNode);

      currentWebDAVServer = &serverNode->webdavServer;
    }
    else if (String_parse(line,STRING_BEGIN,"[device %S]",NULL,name))
    {
      DeviceNode *deviceNode;

      deviceNode = LIST_NEW_NODE(DeviceNode);
      if (deviceNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      deviceNode->name                           = String_duplicate(name);
      deviceNode->device.requestVolumeCommand    = NULL;
      deviceNode->device.unloadVolumeCommand     = NULL;
      deviceNode->device.loadVolumeCommand       = NULL;
      deviceNode->device.volumeSize              = 0LL;
      deviceNode->device.imagePreProcessCommand  = NULL;
      deviceNode->device.imagePostProcessCommand = NULL;
      deviceNode->device.imageCommand            = NULL;
      deviceNode->device.eccPreProcessCommand    = NULL;
      deviceNode->device.eccPostProcessCommand   = NULL;
      deviceNode->device.eccCommand              = NULL;
      deviceNode->device.writePreProcessCommand  = NULL;
      deviceNode->device.writePostProcessCommand = NULL;
      deviceNode->device.writeCommand            = NULL;

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
    else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
      String_unquote(value,STRING_QUOTES);
      if (!ConfigValue_parse(String_cString(name),
                             String_cString(value),
                             CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES),
                             NULL,
                             NULL,
                             NULL
                            )
         )
      {
        if (printInfoFlag) printf("FAIL!\n");
        printError("Unknown or invalid config '%s' with value '%s' in %s, line %ld\n",
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
#warning use errorMessage?

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
#warning use errorMessage?

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
* Purpose: command line option call back for parsing pattern
*          patterns
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParsePattern(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  PatternTypes patternType;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // append to list
  if (PatternList_appendCString((PatternList*)variable,value,patternType) != ERROR_NONE)
  {
    fprintf(stderr,"Cannot parse varlue '%s' of option '%s'!\n",value,name);
    return FALSE;
  }

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

LOCAL bool parseBandWidthNumber(const String s, ulong *n)
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
* Name   : parseDateTimeNumber
* Purpose: parse date/time number (year, day, month, hour, minute)
* Input  : s - string to parse
* Output : n - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseDateTimeNumber(const String s, int *n)
{
  long nextIndex;

  assert(s != NULL);
  assert(n != NULL);

  // init variables
  if   (String_equalsCString(s,"*"))
  {
    (*n) = SCHEDULE_ANY;
  }
  else
  {
    while ((String_length(s) > 1) && String_startsWithChar(s,'0'))
    {
      String_remove(s,STRING_BEGIN,1);
    }
    (*n) = (int)String_toInteger(s,0,&nextIndex,NULL,0);
    if (nextIndex != STRING_END) return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : parseDateMonth
* Purpose: parse date month name
* Input  : s - string to parse
* Output : month - month (MONTH_JAN..MONTH_DEC)
* Return : TRUE iff month parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseDateMonth(const String s, int *month)
{
  String name;
  long   nextIndex;

  assert(s != NULL);
  assert(month != NULL);

  name = String_toLower(String_duplicate(s));
  if      (String_equalsCString(s,"*"))
  {
    (*month) = SCHEDULE_ANY;
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
    while ((String_length(s) > 1) && String_startsWithChar(s,'0'))
    {
      String_remove(s,STRING_BEGIN,1);
    }
    (*month) = (uint)String_toInteger(s,0,&nextIndex,NULL,0);
    if ((nextIndex != STRING_END) || ((*month) < 1) || ((*month) > 12))
    {
      return FALSE;
    }
  }
  String_delete(name);

  return TRUE;
}

/***********************************************************************\
* Name   : parseDateWeekDays
* Purpose: parse date week day
* Input  : s - string to parse
* Output : weekDays - week days
* Return : TRUE iff week day parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseDateWeekDays(const String s, long *weekDays)
{
  String          names;
  StringTokenizer stringTokenizer;
  String          name;

  assert(s != NULL);
  assert(weekDays != NULL);

  if (String_equalsCString(s,"*"))
  {
    (*weekDays) = SCHEDULE_ANY_DAY;
  }
  else
  {
    SET_CLEAR(*weekDays);

    names = String_toLower(String_duplicate(s));
    String_initTokenizer(&stringTokenizer,
                         names,
                         STRING_BEGIN,
                         ",",
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&name,NULL))
    {
      if      (String_equalsIgnoreCaseCString(name,"mon")) SET_ADD(*weekDays,WEEKDAY_MON);
      else if (String_equalsIgnoreCaseCString(name,"tue")) SET_ADD(*weekDays,WEEKDAY_TUE);
      else if (String_equalsIgnoreCaseCString(name,"wed")) SET_ADD(*weekDays,WEEKDAY_WED);
      else if (String_equalsIgnoreCaseCString(name,"thu")) SET_ADD(*weekDays,WEEKDAY_THU);
      else if (String_equalsIgnoreCaseCString(name,"fri")) SET_ADD(*weekDays,WEEKDAY_FRI);
      else if (String_equalsIgnoreCaseCString(name,"sat")) SET_ADD(*weekDays,WEEKDAY_SAT);
      else if (String_equalsIgnoreCaseCString(name,"sun")) SET_ADD(*weekDays,WEEKDAY_SUN);
      else
      {
        String_doneTokenizer(&stringTokenizer);
        String_delete(names);
        return FALSE;
      }
    }
    String_doneTokenizer(&stringTokenizer);
    String_delete(names);
  }

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
  bandWidthNode->year        = SCHEDULE_ANY;
  bandWidthNode->month       = SCHEDULE_ANY;
  bandWidthNode->day         = SCHEDULE_ANY;
  bandWidthNode->hour        = SCHEDULE_ANY;
  bandWidthNode->minute      = SCHEDULE_ANY;
  bandWidthNode->weekDays    = SCHEDULE_ANY_DAY;
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

LOCAL BandWidthNode *parseBandWidth(const String s)
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
  if      (   String_matchCString(s,nextIndex,"^\\s*[[:digit:]]+\\s*$",&nextIndex,s0)
           || String_matchCString(s,nextIndex,"^\\s*[[:digit:]]+\\S+",&nextIndex,s0)
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
        if (!parseDateWeekDays  (s0,&bandWidthNode->weekDays)) errorFlag = TRUE;
        if (!parseDateTimeNumber(s1,&bandWidthNode->hour    )) errorFlag = TRUE;
        if (!parseDateTimeNumber(s2,&bandWidthNode->minute  )) errorFlag = TRUE;
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
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

  // parse band width node
  s = String_newCString(value);
  bandWidthNode = parseBandWidth(s);
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
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

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
#warning use errorMessage?

  if ((*(Password**)variable) == NULL)
  {
    (*(Password**)variable) = Password_new();
  }
  Password_setCString(*(Password**)variable,value);

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseCompressAlgorithm
* Purpose: command line option call back for parsing compress algorithm
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseCompressAlgorithm(void *userData, void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize)
{
  char               algorithm1[256],algorithm2[256];
  CompressAlgorithms compressAlgorithmDelta,compressAlgorithmByte;
  bool               foundFlag;
  uint               z;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

  compressAlgorithmDelta = COMPRESS_ALGORITHM_NONE;
  compressAlgorithmByte  = COMPRESS_ALGORITHM_NONE;

  // parse
  if (   String_scanCString(value,"%256s+%256s",algorithm1,algorithm2)
      || String_scanCString(value,"%256s,%256s",algorithm1,algorithm2)
     )
  {
    foundFlag = FALSE;
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_DELTA); z++)
    {
      if (strcasecmp(algorithm1,COMPRESS_ALGORITHMS_DELTA[z].name) == 0)
      {
        compressAlgorithmDelta = COMPRESS_ALGORITHMS_DELTA[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_BYTE); z++)
    {
      if (strcasecmp(algorithm1,COMPRESS_ALGORITHMS_BYTE[z].name) == 0)
      {
        compressAlgorithmByte = COMPRESS_ALGORITHMS_BYTE[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag) return FALSE;

    foundFlag = FALSE;
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_DELTA); z++)
    {
      if (strcasecmp(algorithm2,COMPRESS_ALGORITHMS_DELTA[z].name) == 0)
      {
        compressAlgorithmDelta = COMPRESS_ALGORITHMS_DELTA[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_BYTE); z++)
    {
      if (strcasecmp(algorithm2,COMPRESS_ALGORITHMS_BYTE[z].name) == 0)
      {
        compressAlgorithmByte = COMPRESS_ALGORITHMS_BYTE[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag) return FALSE;
  }
  else
  {
    foundFlag = FALSE;
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_DELTA); z++)
    {
      if (strcasecmp(value,COMPRESS_ALGORITHMS_DELTA[z].name) == 0)
      {
        compressAlgorithmDelta = COMPRESS_ALGORITHMS_DELTA[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_BYTE); z++)
    {
      if (strcasecmp(value,COMPRESS_ALGORITHMS_BYTE[z].name) == 0)
      {
        compressAlgorithmByte = COMPRESS_ALGORITHMS_BYTE[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag) return FALSE;
  }

  // store compress algorithm values
  ((JobOptionsCompressAlgorithm*)variable)->delta = compressAlgorithmDelta;
  ((JobOptionsCompressAlgorithm*)variable)->byte  = compressAlgorithmByte;

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
#warning use errorMessage?

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
  printf("Usage: %s [<options>] [--] <archive name> [<files>...]\n",programName);
  printf("       %s [<options>] [--] <key file name>\n",programName);
  printf("\n");
  printf("Archive name:  <file name>\n");
  printf("               file://<file name>\n");
  printf("               ftp|scp|sftp|webdav://[<login name>[:<password>]@]<host name>[:<port>]/<file name>\n");
  printf("               cd|dvd|bd|device://[<device name>:]<file name>\n");
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
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

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
  globalOptions.ftpServer                                       = globalOptions.defaultFTPServer;
//  globalOptions.ftpServerList                                   = &ftpServerList;
  globalOptions.defaultFTPServer                                = &defaultFTPServer;
  globalOptions.sshServer                                       = globalOptions.defaultSSHServer;
//  globalOptions.sshServerList                                   = &sshServerList;
  globalOptions.defaultSSHServer                                = &defaultSSHServer;
  globalOptions.webdavServer                                    = globalOptions.defaultWebDAVServer;
//  globalOptions.webdavServerList                                = &webdavServerList;
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
  globalOptions.cd.volumeSize                                   = 0LL;
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
  globalOptions.dvd.volumeSize                                  = 0LL;
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
  globalOptions.bd.volumeSize                                   = 0LL;
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

  switch (serverNode->type)
  {
    case SERVER_TYPE_FTP:
      Password_delete(serverNode->ftpServer.password);
      String_delete(serverNode->ftpServer.loginName);
      break;
    case SERVER_TYPE_SSH:
      String_delete(serverNode->sshServer.privateKeyFileName);
      String_delete(serverNode->sshServer.publicKeyFileName);
      Password_delete(serverNode->sshServer.password);
      String_delete(serverNode->sshServer.loginName);
      break;
    case SERVER_TYPE_WEBDAV:
      String_delete(serverNode->webdavServer.privateKeyFileName);
      String_delete(serverNode->webdavServer.publicKeyFileName);
      Password_delete(serverNode->webdavServer.password);
      String_delete(serverNode->webdavServer.loginName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  doneServerAllocate(&serverNode->serverAllocation);
  String_delete(serverNode->name);
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
* Name   : freeDeviceNode
* Purpose: free device node
* Input  : deviceNode - device node
*          userData   - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeScheduleNode(ScheduleNode *scheduleNode, void *userData)
{
  assert(scheduleNode != NULL);

  UNUSED_VARIABLE(scheduleNode);
  UNUSED_VARIABLE(userData);
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
  Errors error;
  String fileName;

  // initialize crash dump handler
  #if HAVE_BREAKPAD
    if (!MiniDump_init())
    {
      fprintf(stderr,"Warning: Cannot initialize crash dump handler. No crash dumps will be created.\n");
    }
  #endif /* HAVE_BREAKPAD */

  // initialize modules
  error = Password_initAll();
  DEBUG_TEST_CODE("initAll1") { Password_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Crypt_initAll();
  DEBUG_TEST_CODE("initAll2") { Crypt_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Password_doneAll();
    return error;
  }
  error = Pattern_initAll();
  DEBUG_TEST_CODE("initAll3") { Password_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }
  error = PatternList_initAll();
  DEBUG_TEST_CODE("initAll4") { PatternList_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }
  error = Chunk_initAll();
  DEBUG_TEST_CODE("initAll5") { Chunk_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }
  error = Source_initAll();
  DEBUG_TEST_CODE("initAll6") { Source_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Chunk_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }
  error = Archive_initAll();
  DEBUG_TEST_CODE("initAll7") { Archive_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Source_doneAll();
    Chunk_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }
  error = Storage_initAll();
  DEBUG_TEST_CODE("initAll8") { Storage_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Archive_doneAll();
    Source_doneAll();
    Chunk_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }
  error = Index_initAll();
  DEBUG_TEST_CODE("initAll9") { Index_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Storage_doneAll();
    Archive_doneAll();
    Source_doneAll();
    Chunk_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }
  error = Network_initAll();
  DEBUG_TEST_CODE("initAll10") { Network_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Index_doneAll();
    Storage_doneAll();
    Archive_doneAll();
    Source_doneAll();
    Chunk_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }
  error = Server_initAll();
  DEBUG_TEST_CODE("initAll11") { Server_doneAll(); error = ERROR_TESTCODE; }
  if (error != ERROR_NONE)
  {
    Network_doneAll();
    Index_doneAll();
    Storage_doneAll();
    Archive_doneAll();
    Source_doneAll();
    Chunk_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return error;
  }

  // initialize variables
  initGlobalOptions();

  command                               = COMMAND_LIST;
  jobName                               = NULL;
  storageName                           = NULL;
  initJobOptions(&jobOptions);
//  List_init(&ftpServerList);
//  Semaphore_init(&ftpServerListLock);
//  List_init(&sshServerList);
//  Semaphore_init(&sshServerListLock);
//  List_init(&webdavServerList);
//  Semaphore_init(&webdavServerListLock);

  List_init(&serverList);
  Semaphore_init(&serverListLock);

  List_init(&deviceList);
  EntryList_init(&includeEntryList);
  PatternList_init(&excludePatternList);
  PatternList_init(&deltaSourcePatternList);
  PatternList_init(&compressExcludePatternList);
  List_init(&scheduleList);
  defaultFTPServer.loginName             = NULL;
  defaultFTPServer.password              = NULL;
  defaultFTPServer.maxConnectionCount    = MAX_CONNECTION_COUNT_UNLIMITED;
  defaultSSHServer.port                  = 22;
  defaultSSHServer.loginName             = NULL;
  defaultSSHServer.password              = NULL;
  defaultSSHServer.publicKeyFileName     = NULL;
  defaultSSHServer.privateKeyFileName    = NULL;
  defaultSSHServer.maxConnectionCount    = MAX_CONNECTION_COUNT_UNLIMITED;
//  defaultWebDAVServer.port               = 80;
  defaultWebDAVServer.loginName          = NULL;
  defaultWebDAVServer.password           = NULL;
  defaultWebDAVServer.publicKeyFileName  = NULL;
  defaultWebDAVServer.privateKeyFileName = NULL;
  defaultWebDAVServer.maxConnectionCount = MAX_CONNECTION_COUNT_UNLIMITED;
  initServerAllocate(&defaultWebDAVServerAllocation);
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
  logFile                                = NULL;
  tmpLogFile                             = NULL;

  Semaphore_init(&inputLock);
  Semaphore_init(&outputLock);
  Thread_initLocalVariable(&outputLineHandle,outputLineInit,NULL);
  lastOutputLine                         = NULL;

  // initialize default ssh keys
  fileName = File_appendFileNameCString(String_newCString(getenv("HOME")),".ssh/id_rsa.pub");
  if (File_exists(fileName))
  {
    defaultSSHServer.publicKeyFileName = fileName;
  }
  else
  {
    String_delete(fileName);
  }
  fileName = File_appendFileNameCString(String_newCString(getenv("HOME")),".ssh/id_rsa");
  if (File_exists(fileName))
  {
    defaultSSHServer.privateKeyFileName = fileName;
  }
  else
  {
    String_delete(fileName);
  }

  // initialize command line options and config values
  ConfigValue_init(CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES));
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));

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
  Semaphore_done(&outputLock);
  Semaphore_done(&inputLock);
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
  doneServerAllocate(&defaultWebDAVServerAllocation);
  if (defaultWebDAVServer.privateKeyFileName != NULL) String_delete(defaultWebDAVServer.privateKeyFileName);
  if (defaultWebDAVServer.publicKeyFileName != NULL) String_delete(defaultWebDAVServer.publicKeyFileName);
  if (defaultWebDAVServer.password != NULL) Password_delete(defaultWebDAVServer.password);
  if (defaultWebDAVServer.loginName != NULL) String_delete(defaultWebDAVServer.loginName);
  if (defaultSSHServer.privateKeyFileName != NULL) String_delete(defaultSSHServer.privateKeyFileName);
  if (defaultSSHServer.publicKeyFileName != NULL) String_delete(defaultSSHServer.publicKeyFileName);
  if (defaultSSHServer.password != NULL) Password_delete(defaultSSHServer.password);
  if (defaultSSHServer.loginName != NULL) String_delete(defaultSSHServer.loginName);
  if (defaultFTPServer.password != NULL) Password_delete(defaultFTPServer.password);
  if (defaultFTPServer.loginName != NULL) String_delete(defaultFTPServer.loginName);
  List_done(&scheduleList,(ListNodeFreeFunction)freeScheduleNode,NULL);
  PatternList_done(&compressExcludePatternList);
  PatternList_done(&deltaSourcePatternList);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  Password_delete(serverPassword);
  List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);

  Semaphore_done(&serverListLock);
  List_done(&serverList,(ListNodeFreeFunction)freeServerNode,NULL);

  freeJobOptions(&jobOptions);
  String_delete(storageName);
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
  Index_doneAll();
  Storage_doneAll();
  Archive_doneAll();
  Source_doneAll();
  Chunk_doneAll();
  PatternList_doneAll();
  Pattern_doneAll();
  Crypt_doneAll();
  Password_doneAll();

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

/*---------------------------------------------------------------------*/

bool isPrintInfo(uint verboseLevel)
{
  return !globalOptions.quietFlag && ((uint)globalOptions.verboseLevel >= verboseLevel);
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
    String_vformat(line,format,arguments);

    // output
    output(stdout,line);

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
  String  dateTime;
  va_list tmpArguments;

  assert(text != NULL);

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
          fprintf(tmpLogFile,": ");
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
          fprintf(logFile,": ");
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
  return Semaphore_lock(&outputLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,SEMAPHORE_WAIT_FOREVER);
}

void unlockConsole(void)
{
  Semaphore_unlock(&outputLock);
}

void printConsole(FILE *file, const char *format, ...)
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

  // output
  output(file,line);

  String_delete(line);
}

void printWarning(const char *text, ...)
{
  va_list arguments;
  String  line;

  assert(text != NULL);

  // output log line
  va_start(arguments,text);
  vlogMessage(LOG_TYPE_WARNING,"Warning",text,arguments);
  va_end(arguments);

  line = String_new();

  // format line
  va_start(arguments,text);
  String_appendCString(line,"Warning: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  // output
  output(stdout,line);

  String_delete(line);
}

void printError(const char *text, ...)
{
  va_list arguments;
  String  line;

  assert(text != NULL);

  // output log line
  va_start(arguments,text);
  vlogMessage(LOG_TYPE_ERROR,"ERROR",text,arguments);
  va_end(arguments);

  line = String_new();

  // format line
  va_start(arguments,text);
  String_appendCString(line,"ERROR: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  // output console
  output(stderr,line);

  String_delete(line);
}

/***********************************************************************\
* Name   : executeIOlogPostProcess
* Purpose: process log-post command stderr output
* Input  : stringList - strerr string list
*          line       - line
* Output : -
* Return : -
* Notes  : string list will be shortend to last 5 entries
\***********************************************************************/

LOCAL void executeIOlogPostProcess(StringList   *stringList,
                                   const String line
                                  )
{
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
  TextMacro  textMacros[1];
  StringList stderrList;
  Errors     error;
  StringNode *stringNode;
  String     string;

  // flush log
  if (logFile != NULL) fflush(logFile);

  // close temporary log file
  if (tmpLogFile != NULL) fclose(tmpLogFile); tmpLogFile = NULL;

  // log post command for temporary log file
  if (logPostCommand != NULL)
  {
    TEXT_MACRO_N_STRING(textMacros[0],"%file",tmpLogFileName);

    StringList_init(&stderrList);
    error = Misc_executeCommand(logPostCommand,
                                textMacros,SIZE_OF_ARRAY(textMacros),
                                NULL,
                                (ExecuteIOFunction)executeIOlogPostProcess,
                                &stderrList
                               );
    DEBUG_TEST_CODE("logPostProcess") { StringList_done(&stderrList); error = ERROR_TESTCODE; }
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

void initJobOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  memset(jobOptions,0,sizeof(JobOptions));
  jobOptions->archiveType                     = ARCHIVE_TYPE_NORMAL;
  jobOptions->archivePartSize                 = 0LL;
  jobOptions->directoryStripCount             = 0;
  jobOptions->owner.userId                    = FILE_DEFAULT_USER_ID;
  jobOptions->owner.groupId                   = FILE_DEFAULT_GROUP_ID;
  jobOptions->patternType                     = PATTERN_TYPE_GLOB;
  jobOptions->compressAlgorithm.delta         = COMPRESS_ALGORITHM_NONE;
  jobOptions->compressAlgorithm.byte          = COMPRESS_ALGORITHM_NONE;
  jobOptions->cryptAlgorithm                  = CRYPT_ALGORITHM_NONE;
  #ifdef HAVE_GCRYPT
    jobOptions->cryptType                     = CRYPT_TYPE_SYMMETRIC;
  #else /* not HAVE_GCRYPT */
    jobOptions->cryptType                     = CRYPT_TYPE_NONE;
  #endif /* HAVE_GCRYPT */
  jobOptions->cryptPasswordMode               = PASSWORD_MODE_DEFAULT;
  jobOptions->cryptPublicKeyFileName          = NULL;
  jobOptions->cryptPrivateKeyFileName         = NULL;
  jobOptions->ftpServer.maxConnectionCount    = MAX_CONNECTION_COUNT_UNLIMITED;
  jobOptions->sshServer.maxConnectionCount    = MAX_CONNECTION_COUNT_UNLIMITED;
  jobOptions->webdavServer.maxConnectionCount = MAX_CONNECTION_COUNT_UNLIMITED;
  jobOptions->volumeSize                      = 0LL;
  jobOptions->skipUnreadableFlag              = TRUE;
  jobOptions->forceDeltaCompressionFlag       = FALSE;
  jobOptions->ignoreNoDumpAttributeFlag       = FALSE;
  jobOptions->overwriteArchiveFilesFlag       = FALSE;
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

  toJobOptions->ftpServer.loginName                 = String_duplicate(fromJobOptions->ftpServer.loginName);
  toJobOptions->ftpServer.password                  = Password_duplicate(fromJobOptions->ftpServer.password);

  toJobOptions->sshServer.loginName                 = String_duplicate(fromJobOptions->sshServer.loginName);
  toJobOptions->sshServer.password                  = Password_duplicate(fromJobOptions->sshServer.password);
  toJobOptions->sshServer.publicKeyFileName         = String_duplicate(fromJobOptions->sshServer.publicKeyFileName);
  toJobOptions->sshServer.privateKeyFileName        = String_duplicate(fromJobOptions->sshServer.privateKeyFileName);

  toJobOptions->webdavServer.loginName              = String_duplicate(fromJobOptions->webdavServer.loginName);
  toJobOptions->webdavServer.password               = Password_duplicate(fromJobOptions->webdavServer.password);
  toJobOptions->webdavServer.publicKeyFileName      = String_duplicate(fromJobOptions->webdavServer.publicKeyFileName);
  toJobOptions->webdavServer.privateKeyFileName     = String_duplicate(fromJobOptions->webdavServer.privateKeyFileName);

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

void freeJobOptions(JobOptions *jobOptions)
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

  String_delete(jobOptions->webdavServer.privateKeyFileName);
  String_delete(jobOptions->webdavServer.publicKeyFileName);
  Password_delete(jobOptions->webdavServer.password);
  String_delete(jobOptions->webdavServer.loginName);

  String_delete(jobOptions->sshServer.privateKeyFileName);
  String_delete(jobOptions->sshServer.publicKeyFileName);
  Password_delete(jobOptions->sshServer.password);
  String_delete(jobOptions->sshServer.loginName);

  Password_delete(jobOptions->ftpServer.password);
  String_delete(jobOptions->ftpServer.loginName);

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
  bool          readFlag;
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
                       || (   (   (bandWidthNode->year == SCHEDULE_ANY)
                               || (   (currentYear >= (uint)bandWidthNode->year)
                                   && (   (matchingBandWidthNode->year == SCHEDULE_ANY)
                                       || (bandWidthNode->year > matchingBandWidthNode->year)
                                      )
                                  )
                              )
                           && (   (bandWidthNode->month == SCHEDULE_ANY)
                               || (   (bandWidthNode->year == SCHEDULE_ANY)
                                   || (currentYear > (uint)bandWidthNode->year)
                                   || (   (currentMonth >= (uint)bandWidthNode->month)
                                       && (   (matchingBandWidthNode->month == SCHEDULE_ANY)
                                           || (bandWidthNode->month > matchingBandWidthNode->month)
                                          )
                                      )
                                  )
                              )
                           && (   (bandWidthNode->day    == SCHEDULE_ANY)
                               || (   (bandWidthNode->month == SCHEDULE_ANY)
                                   || (currentMonth > (uint)bandWidthNode->month)
                                   || (   (currentDay >= (uint)bandWidthNode->day)
                                       && (   (matchingBandWidthNode->day == SCHEDULE_ANY)
                                           || (bandWidthNode->day > matchingBandWidthNode->day)
                                          )
                                      )
                                  )
                              )
                          );

    // check week day
    weekDayMatchFlag =    (matchingBandWidthNode == NULL)
                       || (   (bandWidthNode->weekDays == SCHEDULE_ANY_DAY)
                           && IN_SET(bandWidthNode->weekDays,currentWeekDay)
                          );

    // check time
    timeMatchFlag =    (matchingBandWidthNode == NULL)
                    || (   (   (bandWidthNode->hour  == SCHEDULE_ANY)
                            || (   (currentHour >= (uint)bandWidthNode->hour)
                                && (   (matchingBandWidthNode->hour == SCHEDULE_ANY)
                                    || (bandWidthNode->hour > matchingBandWidthNode->hour)
                                    )
                               )
                           )
                        && (   (bandWidthNode->minute == SCHEDULE_ANY)
                            || (   (bandWidthNode->hour == SCHEDULE_ANY)
                                || (currentHour > (uint)bandWidthNode->hour)
                                || (   (currentMinute >= (uint)bandWidthNode->minute)
                                    && (   (matchingBandWidthNode->minute == SCHEDULE_ANY)
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
          // read first none-comment line
          readFlag = FALSE;
          line     = String_new();
          while (!File_eof(&fileHandle) && !readFlag)
          {
            // read line
            if (File_readLine(&fileHandle,line) != ERROR_NONE)
            {
              continue;
            }
            String_trim(line,STRING_WHITE_SPACES);

            // skip empty and commented lines (lines starting with //, #, ;)
            if (   String_isEmpty(line)
                || String_startsWithCString(line,"//")
                || String_startsWithCString(line,"#")
                || String_startsWithCString(line,";")
               )
            {
              continue;
            }

            // parse band width
            if (!parseBandWidthNumber(line,&bandWidthList->n))
            {
              continue;
            }

            // value found
            readFlag = TRUE;
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

bool allocateServer(ServerAllocation *serverAllocation, ServerAllocationPriorities priority, int maxConnectionCount)
{
  SemaphoreLock semaphoreLock;

  assert(serverAllocation != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverAllocation->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    switch (priority)
    {
      case SERVER_ALLOCATION_PRIORITY_LOW:
        if (   (maxConnectionCount != MAX_CONNECTION_COUNT_UNLIMITED)
            && (serverAllocation->connectionCount >= (uint)maxConnectionCount)
           )
        {
          // request low priority connection
          serverAllocation->lowPriorityRequestCount++;

          // wait for free connection
          while (serverAllocation->connectionCount >= (uint)maxConnectionCount)
          {
            Semaphore_waitModified(&serverAllocation->lock,SEMAPHORE_WAIT_FOREVER);
          }

          // low priority request done
          assert(serverAllocation->lowPriorityRequestCount > 0);
          serverAllocation->lowPriorityRequestCount--;
        }
        break;
      case SERVER_ALLOCATION_PRIORITY_HIGH:
        if (   (maxConnectionCount != MAX_CONNECTION_COUNT_UNLIMITED)
            && (serverAllocation->connectionCount >= (uint)maxConnectionCount)
           )
        {
          // request high priority connection
          serverAllocation->highPriorityRequestCount++;

          // wait for free connection
          while (serverAllocation->connectionCount >= (uint)maxConnectionCount)
          {
            Semaphore_waitModified(&serverAllocation->lock,SEMAPHORE_WAIT_FOREVER);
          }

          // high priority request done
          assert(serverAllocation->highPriorityRequestCount > 0);
          serverAllocation->highPriorityRequestCount--;
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }

    // allocated connection
    serverAllocation->connectionCount++;
  }

  return TRUE;
}

void freeServer(ServerAllocation *serverAllocation)
{
  SemaphoreLock semaphoreLock;

  assert(serverAllocation != NULL);
  assert(serverAllocation->connectionCount > 0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverAllocation->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // free connection
    serverAllocation->connectionCount--;
  }
}

bool isServerAllocationPending(ServerAllocation *serverAllocation, ServerAllocationPriorities priority)
{
  SemaphoreLock semaphoreLock;
  bool          pendingFlag;

  assert(serverAllocation != NULL);

  pendingFlag = FALSE;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverAllocation->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    switch (priority)
    {
      case SERVER_ALLOCATION_PRIORITY_LOW:
        pendingFlag = (serverAllocation->lowPriorityRequestCount);
        break;
      case SERVER_ALLOCATION_PRIORITY_HIGH:
        pendingFlag = (serverAllocation->highPriorityRequestCount);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
  }

  return pendingFlag;
}

ServerAllocation *getFTPServerSettings(const String     hostName,
                                       const JobOptions *jobOptions,
                                       FTPServer        *ftpServer
                                      )
{
  ServerNode *serverNode;

  assert(hostName != NULL);
  assert(jobOptions != NULL);
  assert(ftpServer != NULL);

  // find FTP server
  serverNode = globalOptions.serverList->head;
  while (   (serverNode != NULL)
         && (   (serverNode->type != SERVER_TYPE_FTP)
             || !String_equals(serverNode->name,hostName)
            )
        )
  {
    serverNode = serverNode->next;
  }

  // get FTP server settings
  ftpServer->loginName          = !String_isEmpty(jobOptions->ftpServer.loginName                            ) ? jobOptions->ftpServer.loginName          : ((serverNode != NULL) ? serverNode->ftpServer.loginName          : globalOptions.defaultFTPServer->loginName         );
  ftpServer->password           = !Password_isEmpty(jobOptions->ftpServer.password                           ) ? jobOptions->ftpServer.password           : ((serverNode != NULL) ? serverNode->ftpServer.password           : globalOptions.defaultFTPServer->password          );
  ftpServer->maxConnectionCount = (jobOptions->ftpServer.maxConnectionCount != MAX_CONNECTION_COUNT_UNLIMITED) ? jobOptions->ftpServer.maxConnectionCount : ((serverNode != NULL) ? serverNode->ftpServer.maxConnectionCount : globalOptions.defaultFTPServer->maxConnectionCount);

  return (serverNode != NULL) ? &serverNode->serverAllocation : &defaultFTPServerAllocation;
}

ServerAllocation *getSSHServerSettings(const String     hostName,
                                       const JobOptions *jobOptions,
                                       SSHServer        *sshServer
                                      )
{
  ServerNode *serverNode;

  assert(hostName != NULL);
  assert(jobOptions != NULL);
  assert(sshServer != NULL);

  // find SSH server
  serverNode = globalOptions.serverList->head;
  while (   (serverNode != NULL)
         && (   (serverNode->type != SERVER_TYPE_SSH)
             || !String_equals(serverNode->name,hostName)
            )
        )
  {
    serverNode = serverNode->next;
  }

  // get SSH server settings
  sshServer->port               = (jobOptions->sshServer.port != 0                                           ) ? jobOptions->sshServer.port               : ((serverNode != NULL) ? serverNode->sshServer.port               : globalOptions.defaultSSHServer->port              );
  sshServer->loginName          = !String_isEmpty(jobOptions->sshServer.loginName                            ) ? jobOptions->sshServer.loginName          : ((serverNode != NULL) ? serverNode->sshServer.loginName          : globalOptions.defaultSSHServer->loginName         );
  sshServer->password           = !Password_isEmpty(jobOptions->sshServer.password                           ) ? jobOptions->sshServer.password           : ((serverNode != NULL) ? serverNode->sshServer.password           : globalOptions.defaultSSHServer->password          );
  sshServer->publicKeyFileName  = !String_isEmpty(jobOptions->sshServer.publicKeyFileName                    ) ? jobOptions->sshServer.publicKeyFileName  : ((serverNode != NULL) ? serverNode->sshServer.publicKeyFileName  : globalOptions.defaultSSHServer->publicKeyFileName );
  sshServer->privateKeyFileName = !String_isEmpty(jobOptions->sshServer.privateKeyFileName                   ) ? jobOptions->sshServer.privateKeyFileName : ((serverNode != NULL) ? serverNode->sshServer.privateKeyFileName : globalOptions.defaultSSHServer->privateKeyFileName);
  sshServer->maxConnectionCount = (jobOptions->sshServer.maxConnectionCount != MAX_CONNECTION_COUNT_UNLIMITED) ? jobOptions->sshServer.maxConnectionCount : ((serverNode != NULL) ? serverNode->sshServer.maxConnectionCount : globalOptions.defaultSSHServer->maxConnectionCount);

  return (serverNode != NULL) ? &serverNode->serverAllocation : &defaultSSHServerAllocation;
}

ServerAllocation *getWebDAVServerSettings(const String     hostName,
                                          const JobOptions *jobOptions,
                                          WebDAVServer     *webdavServer
                                         )
{
  ServerNode *serverNode;

  assert(hostName != NULL);
  assert(jobOptions != NULL);
  assert(webdavServer != NULL);

  // find WebDAV server
  serverNode = globalOptions.serverList->head;
  while (   (serverNode != NULL)
         && (   (serverNode->type != SERVER_TYPE_WEBDAV)
             || !String_equals(serverNode->name,hostName)
            )
        )
  {
    serverNode = serverNode->next;
  }

  // get WebDAV server settings
//  webdavServer->port               = (jobOptions->webdavServer.port != 0                                           ) ? jobOptions->webdavServer.port               : ((serverNode != NULL) ? serverNode->webdavServer.port               : globalOptions.defaultWebDAVServer->port              );
  webdavServer->loginName          = !String_isEmpty(jobOptions->webdavServer.loginName                            ) ? jobOptions->webdavServer.loginName          : ((serverNode != NULL) ? serverNode->webdavServer.loginName          : globalOptions.defaultWebDAVServer->loginName         );
  webdavServer->password           = !Password_isEmpty(jobOptions->webdavServer.password                           ) ? jobOptions->webdavServer.password           : ((serverNode != NULL) ? serverNode->webdavServer.password           : globalOptions.defaultWebDAVServer->password          );
  webdavServer->publicKeyFileName  = !String_isEmpty(jobOptions->webdavServer.publicKeyFileName                    ) ? jobOptions->webdavServer.publicKeyFileName  : ((serverNode != NULL) ? serverNode->webdavServer.publicKeyFileName  : globalOptions.defaultWebDAVServer->publicKeyFileName );
  webdavServer->privateKeyFileName = !String_isEmpty(jobOptions->webdavServer.privateKeyFileName                   ) ? jobOptions->webdavServer.privateKeyFileName : ((serverNode != NULL) ? serverNode->webdavServer.privateKeyFileName : globalOptions.defaultWebDAVServer->privateKeyFileName);
  webdavServer->maxConnectionCount = (jobOptions->webdavServer.maxConnectionCount != MAX_CONNECTION_COUNT_UNLIMITED) ? jobOptions->webdavServer.maxConnectionCount : ((serverNode != NULL) ? serverNode->webdavServer.maxConnectionCount : globalOptions.defaultWebDAVServer->maxConnectionCount);

  return (serverNode != NULL) ? &serverNode->serverAllocation : &defaultWebDAVServerAllocation;
}

void getCDSettings(const JobOptions *jobOptions,
                   OpticalDisk      *opticalDisk
                  )
{
  assert(jobOptions != NULL);
  assert(opticalDisk != NULL);

  opticalDisk->defaultDeviceName       = (jobOptions->opticalDisk.defaultDeviceName       != NULL)?jobOptions->opticalDisk.defaultDeviceName      :globalOptions.cd.defaultDeviceName;
  opticalDisk->requestVolumeCommand    = (jobOptions->opticalDisk.requestVolumeCommand    != NULL)?jobOptions->opticalDisk.requestVolumeCommand   :globalOptions.cd.requestVolumeCommand;
  opticalDisk->unloadVolumeCommand     = (jobOptions->opticalDisk.unloadVolumeCommand     != NULL)?jobOptions->opticalDisk.unloadVolumeCommand    :globalOptions.cd.unloadVolumeCommand;
  opticalDisk->loadVolumeCommand       = (jobOptions->opticalDisk.loadVolumeCommand       != NULL)?jobOptions->opticalDisk.loadVolumeCommand      :globalOptions.cd.loadVolumeCommand;
  opticalDisk->volumeSize              = (jobOptions->opticalDisk.volumeSize              != 0LL )?jobOptions->opticalDisk.volumeSize             :globalOptions.cd.volumeSize;
  opticalDisk->imagePreProcessCommand  = (jobOptions->opticalDisk.imagePreProcessCommand  != NULL)?jobOptions->opticalDisk.imagePreProcessCommand :globalOptions.cd.imagePreProcessCommand;
  opticalDisk->imagePostProcessCommand = (jobOptions->opticalDisk.imagePostProcessCommand != NULL)?jobOptions->opticalDisk.imagePostProcessCommand:globalOptions.cd.imagePostProcessCommand;
  opticalDisk->imageCommand            = (jobOptions->opticalDisk.imageCommand            != NULL)?jobOptions->opticalDisk.imageCommand           :globalOptions.cd.imageCommand;
  opticalDisk->eccPreProcessCommand    = (jobOptions->opticalDisk.eccPreProcessCommand    != NULL)?jobOptions->opticalDisk.eccPreProcessCommand   :globalOptions.cd.eccPreProcessCommand;
  opticalDisk->eccPostProcessCommand   = (jobOptions->opticalDisk.eccPostProcessCommand   != NULL)?jobOptions->opticalDisk.eccPostProcessCommand  :globalOptions.cd.eccPostProcessCommand;
  opticalDisk->eccCommand              = (jobOptions->opticalDisk.eccCommand              != NULL)?jobOptions->opticalDisk.eccCommand             :globalOptions.cd.eccCommand;
  opticalDisk->writePreProcessCommand  = (jobOptions->opticalDisk.writePreProcessCommand  != NULL)?jobOptions->opticalDisk.writePreProcessCommand :globalOptions.cd.writePreProcessCommand;
  opticalDisk->writePostProcessCommand = (jobOptions->opticalDisk.writePostProcessCommand != NULL)?jobOptions->opticalDisk.writePostProcessCommand:globalOptions.cd.writePostProcessCommand;
  opticalDisk->writeCommand            = (jobOptions->opticalDisk.writeCommand            != NULL)?jobOptions->opticalDisk.writeCommand           :globalOptions.cd.writeCommand;
  opticalDisk->writeImageCommand       = (jobOptions->opticalDisk.writeImageCommand       != NULL)?jobOptions->opticalDisk.writeImageCommand      :globalOptions.cd.writeImageCommand;
}

void getDVDSettings(const JobOptions *jobOptions,
                    OpticalDisk      *opticalDisk
                   )
{
  assert(jobOptions != NULL);
  assert(opticalDisk != NULL);

  opticalDisk->defaultDeviceName       = (jobOptions->opticalDisk.defaultDeviceName       != NULL)?jobOptions->opticalDisk.defaultDeviceName      :globalOptions.dvd.defaultDeviceName;
  opticalDisk->requestVolumeCommand    = (jobOptions->opticalDisk.requestVolumeCommand    != NULL)?jobOptions->opticalDisk.requestVolumeCommand   :globalOptions.dvd.requestVolumeCommand;
  opticalDisk->unloadVolumeCommand     = (jobOptions->opticalDisk.unloadVolumeCommand     != NULL)?jobOptions->opticalDisk.unloadVolumeCommand    :globalOptions.dvd.unloadVolumeCommand;
  opticalDisk->loadVolumeCommand       = (jobOptions->opticalDisk.loadVolumeCommand       != NULL)?jobOptions->opticalDisk.loadVolumeCommand      :globalOptions.dvd.loadVolumeCommand;
  opticalDisk->volumeSize              = (jobOptions->opticalDisk.volumeSize              != 0LL )?jobOptions->opticalDisk.volumeSize             :globalOptions.dvd.volumeSize;
  opticalDisk->imagePreProcessCommand  = (jobOptions->opticalDisk.imagePreProcessCommand  != NULL)?jobOptions->opticalDisk.imagePreProcessCommand :globalOptions.dvd.imagePreProcessCommand;
  opticalDisk->imagePostProcessCommand = (jobOptions->opticalDisk.imagePostProcessCommand != NULL)?jobOptions->opticalDisk.imagePostProcessCommand:globalOptions.dvd.imagePostProcessCommand;
  opticalDisk->imageCommand            = (jobOptions->opticalDisk.imageCommand            != NULL)?jobOptions->opticalDisk.imageCommand           :globalOptions.dvd.imageCommand;
  opticalDisk->eccPreProcessCommand    = (jobOptions->opticalDisk.eccPreProcessCommand    != NULL)?jobOptions->opticalDisk.eccPreProcessCommand   :globalOptions.dvd.eccPreProcessCommand;
  opticalDisk->eccPostProcessCommand   = (jobOptions->opticalDisk.eccPostProcessCommand   != NULL)?jobOptions->opticalDisk.eccPostProcessCommand  :globalOptions.dvd.eccPostProcessCommand;
  opticalDisk->eccCommand              = (jobOptions->opticalDisk.eccCommand              != NULL)?jobOptions->opticalDisk.eccCommand             :globalOptions.dvd.eccCommand;
  opticalDisk->writePreProcessCommand  = (jobOptions->opticalDisk.writePreProcessCommand  != NULL)?jobOptions->opticalDisk.writePreProcessCommand :globalOptions.dvd.writePreProcessCommand;
  opticalDisk->writePostProcessCommand = (jobOptions->opticalDisk.writePostProcessCommand != NULL)?jobOptions->opticalDisk.writePostProcessCommand:globalOptions.dvd.writePostProcessCommand;
  opticalDisk->writeCommand            = (jobOptions->opticalDisk.writeCommand            != NULL)?jobOptions->opticalDisk.writeCommand           :globalOptions.dvd.writeCommand;
  opticalDisk->writeImageCommand       = (jobOptions->opticalDisk.writeImageCommand       != NULL)?jobOptions->opticalDisk.writeImageCommand      :globalOptions.dvd.writeImageCommand;
}

void getBDSettings(const JobOptions *jobOptions,
                   OpticalDisk      *opticalDisk
                  )
{
  assert(jobOptions != NULL);
  assert(opticalDisk != NULL);

  opticalDisk->defaultDeviceName       = (jobOptions->opticalDisk.defaultDeviceName       != NULL)?jobOptions->opticalDisk.defaultDeviceName      :globalOptions.bd.defaultDeviceName;
  opticalDisk->requestVolumeCommand    = (jobOptions->opticalDisk.requestVolumeCommand    != NULL)?jobOptions->opticalDisk.requestVolumeCommand   :globalOptions.bd.requestVolumeCommand;
  opticalDisk->unloadVolumeCommand     = (jobOptions->opticalDisk.unloadVolumeCommand     != NULL)?jobOptions->opticalDisk.unloadVolumeCommand    :globalOptions.bd.unloadVolumeCommand;
  opticalDisk->loadVolumeCommand       = (jobOptions->opticalDisk.loadVolumeCommand       != NULL)?jobOptions->opticalDisk.loadVolumeCommand      :globalOptions.bd.loadVolumeCommand;
  opticalDisk->volumeSize              = (jobOptions->opticalDisk.volumeSize              != 0LL )?jobOptions->opticalDisk.volumeSize             :globalOptions.bd.volumeSize;
  opticalDisk->imagePreProcessCommand  = (jobOptions->opticalDisk.imagePreProcessCommand  != NULL)?jobOptions->opticalDisk.imagePreProcessCommand :globalOptions.bd.imagePreProcessCommand;
  opticalDisk->imagePostProcessCommand = (jobOptions->opticalDisk.imagePostProcessCommand != NULL)?jobOptions->opticalDisk.imagePostProcessCommand:globalOptions.bd.imagePostProcessCommand;
  opticalDisk->imageCommand            = (jobOptions->opticalDisk.imageCommand            != NULL)?jobOptions->opticalDisk.imageCommand           :globalOptions.bd.imageCommand;
  opticalDisk->eccPreProcessCommand    = (jobOptions->opticalDisk.eccPreProcessCommand    != NULL)?jobOptions->opticalDisk.eccPreProcessCommand   :globalOptions.bd.eccPreProcessCommand;
  opticalDisk->eccPostProcessCommand   = (jobOptions->opticalDisk.eccPostProcessCommand   != NULL)?jobOptions->opticalDisk.eccPostProcessCommand  :globalOptions.bd.eccPostProcessCommand;
  opticalDisk->eccCommand              = (jobOptions->opticalDisk.eccCommand              != NULL)?jobOptions->opticalDisk.eccCommand             :globalOptions.bd.eccCommand;
  opticalDisk->writePreProcessCommand  = (jobOptions->opticalDisk.writePreProcessCommand  != NULL)?jobOptions->opticalDisk.writePreProcessCommand :globalOptions.bd.writePreProcessCommand;
  opticalDisk->writePostProcessCommand = (jobOptions->opticalDisk.writePostProcessCommand != NULL)?jobOptions->opticalDisk.writePostProcessCommand:globalOptions.bd.writePostProcessCommand;
  opticalDisk->writeCommand            = (jobOptions->opticalDisk.writeCommand            != NULL)?jobOptions->opticalDisk.writeCommand           :globalOptions.bd.writeCommand;
  opticalDisk->writeImageCommand       = (jobOptions->opticalDisk.writeImageCommand       != NULL)?jobOptions->opticalDisk.writeImageCommand      :globalOptions.bd.writeImageCommand;
}

void getDeviceSettings(const String     name,
                       const JobOptions *jobOptions,
                       Device           *device
                      )
{
  DeviceNode *deviceNode;

  assert(name != NULL);
  assert(jobOptions != NULL);
  assert(device != NULL);

  deviceNode = globalOptions.deviceList->head;
  while ((deviceNode != NULL) && !String_equals(deviceNode->name,name))
  {
    deviceNode = deviceNode->next;
  }
  device->defaultDeviceName       = (jobOptions->device.defaultDeviceName       != NULL)?jobOptions->device.defaultDeviceName      :((deviceNode != NULL)?deviceNode->device.defaultDeviceName      :globalOptions.defaultDevice->defaultDeviceName      );
  device->requestVolumeCommand    = (jobOptions->device.requestVolumeCommand    != NULL)?jobOptions->device.requestVolumeCommand   :((deviceNode != NULL)?deviceNode->device.requestVolumeCommand   :globalOptions.defaultDevice->requestVolumeCommand   );
  device->unloadVolumeCommand     = (jobOptions->device.unloadVolumeCommand     != NULL)?jobOptions->device.unloadVolumeCommand    :((deviceNode != NULL)?deviceNode->device.unloadVolumeCommand    :globalOptions.defaultDevice->unloadVolumeCommand    );
  device->loadVolumeCommand       = (jobOptions->device.loadVolumeCommand       != NULL)?jobOptions->device.loadVolumeCommand      :((deviceNode != NULL)?deviceNode->device.loadVolumeCommand      :globalOptions.defaultDevice->loadVolumeCommand      );
  device->volumeSize              = (jobOptions->device.volumeSize              != 0LL )?jobOptions->device.volumeSize             :((deviceNode != NULL)?deviceNode->device.volumeSize             :globalOptions.defaultDevice->volumeSize             );
  device->imagePreProcessCommand  = (jobOptions->device.imagePreProcessCommand  != NULL)?jobOptions->device.imagePreProcessCommand :((deviceNode != NULL)?deviceNode->device.imagePreProcessCommand :globalOptions.defaultDevice->imagePreProcessCommand );
  device->imagePostProcessCommand = (jobOptions->device.imagePostProcessCommand != NULL)?jobOptions->device.imagePostProcessCommand:((deviceNode != NULL)?deviceNode->device.imagePostProcessCommand:globalOptions.defaultDevice->imagePostProcessCommand);
  device->imageCommand            = (jobOptions->device.imageCommand            != NULL)?jobOptions->device.imageCommand           :((deviceNode != NULL)?deviceNode->device.imageCommand           :globalOptions.defaultDevice->imageCommand           );
  device->eccPreProcessCommand    = (jobOptions->device.eccPreProcessCommand    != NULL)?jobOptions->device.eccPreProcessCommand   :((deviceNode != NULL)?deviceNode->device.eccPreProcessCommand   :globalOptions.defaultDevice->eccPreProcessCommand   );
  device->eccPostProcessCommand   = (jobOptions->device.eccPostProcessCommand   != NULL)?jobOptions->device.eccPostProcessCommand  :((deviceNode != NULL)?deviceNode->device.eccPostProcessCommand  :globalOptions.defaultDevice->eccPostProcessCommand  );
  device->eccCommand              = (jobOptions->device.eccCommand              != NULL)?jobOptions->device.eccCommand             :((deviceNode != NULL)?deviceNode->device.eccCommand             :globalOptions.defaultDevice->eccCommand             );
  device->writePreProcessCommand  = (jobOptions->device.writePreProcessCommand  != NULL)?jobOptions->device.writePreProcessCommand :((deviceNode != NULL)?deviceNode->device.writePreProcessCommand :globalOptions.defaultDevice->writePreProcessCommand );
  device->writePostProcessCommand = (jobOptions->device.writePostProcessCommand != NULL)?jobOptions->device.writePostProcessCommand:((deviceNode != NULL)?deviceNode->device.writePostProcessCommand:globalOptions.defaultDevice->writePostProcessCommand);
  device->writeCommand            = (jobOptions->device.writeCommand            != NULL)?jobOptions->device.writeCommand           :((deviceNode != NULL)?deviceNode->device.writeCommand           :globalOptions.defaultDevice->writeCommand           );
}

Errors inputCryptPassword(void         *userData,
                          Password     *password,
                          const String fileName,
                          bool         validateFlag,
                          bool         weakCheckFlag
                         )
{
  Errors        error;
#warning remove semaphoreLock
  SemaphoreLock semaphoreLock;

  assert(password != NULL);
  assert(fileName != NULL);

  UNUSED_VARIABLE(userData);

  error = ERROR_UNKNOWN;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&inputLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    switch (globalOptions.runMode)
    {
      case RUN_MODE_INTERACTIVE:
        {
          String title;

          lockConsole();
          {
            // input password
            title = String_new();
            if (!String_isEmpty(fileName))
            {
              String_format(title,"Crypt password for '%S'",fileName);
            }
            else
            {
              String_setCString(title,"Crypt password");
            }
            if (!Password_input(password,String_cString(title),PASSWORD_INPUT_MODE_ANY) || (Password_length(password) <= 0))
            {
              String_delete(title);
              unlockConsole();
              error = ERROR_NO_CRYPT_PASSWORD;
              break;
            }
            if (validateFlag)
            {
              // verify input password
              if (!String_isEmpty(fileName))
              {
                String_format(String_clear(title),"Verify password for '%S'",fileName);
              }
              else
              {
                String_setCString(title,"Verify password");
              }
              if (!Password_inputVerify(password,String_cString(title),PASSWORD_INPUT_MODE_ANY))
              {
                printError("Crypt passwords are not equal!\n");
                String_delete(title);
                unlockConsole();
                error = ERROR_CRYPT_PASSWORDS_MISMATCH;
                break;
              }
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
          }
          unlockConsole();

          error = ERROR_NONE;
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
  }

  return error;
}

bool configValueParseBandWidth(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  BandWidthNode *bandWidthNode;
  String        s;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

  // parse band width node
  s = String_newCString(value);
  bandWidthNode = parseBandWidth(s);
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
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

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
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // append to list
  pattern = String_mapCString(String_newCString(value),STRING_BEGIN,FILENAME_MAP_FROM,FILENAME_MAP_TO,SIZE_OF_ARRAY(FILENAME_MAP_FROM));
  error = EntryList_append((EntryList*)variable,entryType,pattern,patternType);
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"Cannot parse varlue '%s' of option '%s'!\n",String_cString(pattern),name);
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
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
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
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
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

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

  // detect pattern type, get pattern
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  // append to list
  if (PatternList_appendCString((PatternList*)variable,value,patternType) != ERROR_NONE)
  {
    fprintf(stderr,"Cannot parse varlue '%s' of option '%s'!\n",value,name);
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
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }

    (*formatUserData) = patternNode->next;

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
#warning use errorMessage?

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

bool configValueParsePassword(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

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

bool configValueParseCompressAlgorithm(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  char               algorithm1[256],algorithm2[256];
  CompressAlgorithms compressAlgorithmDelta,compressAlgorithmByte;
  bool               foundFlag;
  uint               z;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

  compressAlgorithmDelta = COMPRESS_ALGORITHM_NONE;
  compressAlgorithmByte  = COMPRESS_ALGORITHM_NONE;

  // parse
  if (   String_scanCString(value,"%256s+%256s",algorithm1,algorithm2)
      || String_scanCString(value,"%256s,%256s",algorithm1,algorithm2)
     )
  {
    foundFlag = FALSE;
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_DELTA); z++)
    {
      if (strcasecmp(algorithm1,COMPRESS_ALGORITHMS_DELTA[z].name) == 0)
      {
        compressAlgorithmDelta = COMPRESS_ALGORITHMS_DELTA[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_BYTE); z++)
    {
      if (strcasecmp(algorithm1,COMPRESS_ALGORITHMS_BYTE[z].name) == 0)
      {
        compressAlgorithmByte = COMPRESS_ALGORITHMS_BYTE[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag) return FALSE;

    foundFlag = FALSE;
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_DELTA); z++)
    {
      if (strcasecmp(algorithm2,COMPRESS_ALGORITHMS_DELTA[z].name) == 0)
      {
        compressAlgorithmDelta = COMPRESS_ALGORITHMS_DELTA[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_BYTE); z++)
    {
      if (strcasecmp(algorithm2,COMPRESS_ALGORITHMS_BYTE[z].name) == 0)
      {
        compressAlgorithmByte = COMPRESS_ALGORITHMS_BYTE[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag) return FALSE;
  }
  else
  {
    foundFlag = FALSE;
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_DELTA); z++)
    {
      if (strcasecmp(value,COMPRESS_ALGORITHMS_DELTA[z].name) == 0)
      {
        compressAlgorithmDelta = COMPRESS_ALGORITHMS_DELTA[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    for (z = 0; z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS_BYTE); z++)
    {
      if (strcasecmp(value,COMPRESS_ALGORITHMS_BYTE[z].name) == 0)
      {
        compressAlgorithmByte = COMPRESS_ALGORITHMS_BYTE[z].compressAlgorithm;
        foundFlag = TRUE;
        break;
      }
    }
    if (!foundFlag) return FALSE;
  }

  // store compress algorithm values
  ((JobOptionsCompressAlgorithm*)variable)->delta = compressAlgorithmDelta;
  ((JobOptionsCompressAlgorithm*)variable)->byte  = compressAlgorithmByte;

  return TRUE;
}

void configValueFormatInitCompressAlgorithm(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (JobOptionsCompressAlgorithm*)variable;
}

void configValueFormatDoneCompressAlgorithm(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

bool configValueFormatCompressAlgorithm(void **formatUserData, void *userData, String line)
{
  JobOptionsCompressAlgorithm *jobOptionsCompressAlgorithm;

  assert(formatUserData != NULL);
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  jobOptionsCompressAlgorithm = (JobOptionsCompressAlgorithm*)(*formatUserData);
  if (jobOptionsCompressAlgorithm != NULL)
  {
    String_format(line,
                  "%s+%s",
                  Compress_getAlgorithmName(jobOptionsCompressAlgorithm->delta),
                  Compress_getAlgorithmName(jobOptionsCompressAlgorithm->byte)
                 );
    (*formatUserData) = NULL;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/***********************************************************************\
* Name   : newScheduleNode
* Purpose: create new schedule node
* Input  : -
* Output : -
* Return : schedule node
* Notes  : -
\***********************************************************************/

LOCAL ScheduleNode *newScheduleNode(void)
{
  ScheduleNode *scheduleNode;

  // allocate new schedule node
  scheduleNode = LIST_NEW_NODE(ScheduleNode);
  if (scheduleNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  scheduleNode->year        = SCHEDULE_ANY;
  scheduleNode->month       = SCHEDULE_ANY;
  scheduleNode->day         = SCHEDULE_ANY;
  scheduleNode->hour        = SCHEDULE_ANY;
  scheduleNode->minute      = SCHEDULE_ANY;
  scheduleNode->weekDays    = SCHEDULE_ANY_DAY;
  scheduleNode->archiveType = ARCHIVE_TYPE_NORMAL;
  scheduleNode->enabled     = FALSE;

  return scheduleNode;
}

/***********************************************************************\
* Name   : parseScheduleArchiveType
* Purpose: parse archive type
* Input  : s - string to parse
* Output : archiveType - archive type
* Return : TRUE iff archive type parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseScheduleArchiveType(const String s, ArchiveTypes *archiveType)
{
  String name;

  assert(s != NULL);
  assert(archiveType != NULL);

  name = String_toLower(String_duplicate(s));
  if (String_equalsCString(s,"*"))
  {
    (*archiveType) = ARCHIVE_TYPE_NORMAL;
  }
  else if (String_equalsIgnoreCaseCString(name,"normal"      )) (*archiveType) = ARCHIVE_TYPE_NORMAL;
  else if (String_equalsIgnoreCaseCString(name,"full"        )) (*archiveType) = ARCHIVE_TYPE_FULL;
  else if (String_equalsIgnoreCaseCString(name,"incremental" )) (*archiveType) = ARCHIVE_TYPE_INCREMENTAL;
  else if (String_equalsIgnoreCaseCString(name,"differential")) (*archiveType) = ARCHIVE_TYPE_DIFFERENTIAL;
  else
  {
    String_delete(name);
    return FALSE;
  }
  String_delete(name);

  return TRUE;
}

ScheduleNode *parseScheduleParts(const String date,
                                 const String weekDay,
                                 const String time,
                                 const String enabled,
                                 const String archiveType
                                )
{
  ScheduleNode *scheduleNode;
  bool         errorFlag;
  String       s0,s1,s2;
  bool         b;

  assert(date != NULL);
  assert(weekDay != NULL);
  assert(time != NULL);
  assert(enabled != NULL);
  assert(archiveType != NULL);

  // allocate new schedule node
  scheduleNode = newScheduleNode();

  // parse schedule. Format: date [weekday] time enabled [type]
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  if      (String_parse(date,STRING_BEGIN,"%S-%S-%S",NULL,s0,s1,s2))
  {
    if (!parseDateTimeNumber(s0,&scheduleNode->year)) errorFlag = TRUE;
    if (!parseDateMonth     (s1,&scheduleNode->month)) errorFlag = TRUE;
    if (!parseDateTimeNumber(s2,&scheduleNode->day)) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  if (!parseDateWeekDays(weekDay,&scheduleNode->weekDays))
  {
    errorFlag = TRUE;
  }
  if (String_parse(time,STRING_BEGIN,"%S:%S",NULL,s0,s1))
  {
    if (!parseDateTimeNumber(s0,&scheduleNode->hour  )) errorFlag = TRUE;
    if (!parseDateTimeNumber(s1,&scheduleNode->minute)) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  if (String_parse(enabled,STRING_BEGIN,"%y",NULL,&b))
  {
/* It seems gcc has a bug in option -fno-schedule-insns2: if -O2 is used this
   option is enabled. Then either the program crashes with a SigSegV or parsing
   boolean values here fail. It seems the address of 'b' is not received in the
   function. Because this problem disappear when -fno-schedule-insns2 is given
   it looks like the gcc do some rearrangements in the generated machine code
   which is not valid anymore. How can this be tracked down? Is this problem
   known?
*/
if ((b != FALSE) && (b != TRUE)) HALT_INTERNAL_ERROR("parsing boolean string value fail - C compiler bug?");
    scheduleNode->enabled = b;
  }
  else
  {
    errorFlag = TRUE;
  }
  if (!parseScheduleArchiveType(archiveType,&scheduleNode->archiveType)) errorFlag = TRUE;
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);

  if (errorFlag)
  {
    LIST_DELETE_NODE(scheduleNode);
    return NULL;
  }

  return scheduleNode;
}

ScheduleNode *parseSchedule(const String s)
{
  ScheduleNode *scheduleNode;
  bool         errorFlag;
  String       s0,s1,s2;
  bool         b;
  long         nextIndex;

  assert(s != NULL);

  // allocate new schedule node
  scheduleNode = newScheduleNode();

  // parse schedule. Format: date [weekday] time enabled [type]
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  nextIndex = STRING_BEGIN;
  if      (String_parse(s,nextIndex,"%S-%S-%S",&nextIndex,s0,s1,s2))
  {
    if (!parseDateTimeNumber(s0,&scheduleNode->year )) errorFlag = TRUE;
    if (!parseDateMonth     (s1,&scheduleNode->month)) errorFlag = TRUE;
    if (!parseDateTimeNumber(s2,&scheduleNode->day  )) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  if      (String_parse(s,nextIndex,"%S %S:%S",&nextIndex,s0,s1,s2))
  {
    if (!parseDateWeekDays  (s0,&scheduleNode->weekDays)) errorFlag = TRUE;
    if (!parseDateTimeNumber(s1,&scheduleNode->hour    )) errorFlag = TRUE;
    if (!parseDateTimeNumber(s2,&scheduleNode->minute  )) errorFlag = TRUE;
  }
  else if (String_parse(s,nextIndex,"%S:%S",&nextIndex,s0,s1))
  {
    if (!parseDateTimeNumber(s0,&scheduleNode->hour  )) errorFlag = TRUE;
    if (!parseDateTimeNumber(s1,&scheduleNode->minute)) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  if (String_parse(s,nextIndex,"%y",&nextIndex,&b))
  {
/* It seems gcc has a bug in option -fno-schedule-insns2: if -O2 is used this
   option is enabled. Then either the program crashes with a SigSegV or parsing
   boolean values here fail. It seems the address of 'b' is not received in the
   function. Because this problem disappear when -fno-schedule-insns2 is given
   it looks like the gcc do some rearrangements in the generated machine code
   which is not valid anymore. How can this be tracked down? Is this problem
   known?
*/
if ((b != FALSE) && (b != TRUE)) HALT_INTERNAL_ERROR("parsing boolean string value fail - C compiler bug?");
    scheduleNode->enabled = b;
  }
  else
  {
    errorFlag = TRUE;
  }
//fprintf(stderr,"%s,%d: scheduleNode->enabled=%d %p\n",__FILE__,__LINE__,scheduleNode->enabled,&b);
  if (nextIndex != STRING_END)
  {
    if (String_parse(s,nextIndex,"%S",&nextIndex,s0))
    {
      if (!parseScheduleArchiveType(s0,&scheduleNode->archiveType)) errorFlag = TRUE;
    }
  }
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);

  if (errorFlag || (nextIndex != STRING_END))
  {
    LIST_DELETE_NODE(scheduleNode);
    return NULL;
  }

  return scheduleNode;
}

bool configValueParseSchedule(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  ScheduleNode *scheduleNode;
  String       s;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
#warning use errorMessage?

  // parse schedule node
  s = String_newCString(value);
  scheduleNode = parseSchedule(s);
  if (scheduleNode == NULL)
  {
    String_delete(s);
    return FALSE;
  }
  String_delete(s);

  // append to list
  List_append((ScheduleList*)variable,scheduleNode);

  return TRUE;
}

void configValueFormatInitSchedule(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((ScheduleList*)variable)->head;
}

void configValueFormatDoneSchedule(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(formatUserData);
}

bool configValueFormatSchedule(void **formatUserData, void *userData, String line)
{
  ScheduleNode *scheduleNode;
  String       names;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  scheduleNode = (ScheduleNode*)(*formatUserData);
  if (scheduleNode != NULL)
  {
    if (scheduleNode->year != SCHEDULE_ANY)
    {
      String_format(line,"%d",scheduleNode->year);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,'-');
    if (scheduleNode->month != SCHEDULE_ANY)
    {
      String_format(line,"%d",scheduleNode->month);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,'-');
    if (scheduleNode->day != SCHEDULE_ANY)
    {
      String_format(line,"%d",scheduleNode->day);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,' ');

    if (scheduleNode->weekDays != SCHEDULE_ANY_DAY)
    {
      names = String_new();

      if (IN_SET(scheduleNode->weekDays,WEEKDAY_MON)) { String_joinCString(names,"Mon",','); }
      if (IN_SET(scheduleNode->weekDays,WEEKDAY_TUE)) { String_joinCString(names,"Tue",','); }
      if (IN_SET(scheduleNode->weekDays,WEEKDAY_WED)) { String_joinCString(names,"Wed",','); }
      if (IN_SET(scheduleNode->weekDays,WEEKDAY_THU)) { String_joinCString(names,"Thu",','); }
      if (IN_SET(scheduleNode->weekDays,WEEKDAY_FRI)) { String_joinCString(names,"Fri",','); }
      if (IN_SET(scheduleNode->weekDays,WEEKDAY_SAT)) { String_joinCString(names,"Sat",','); }
      if (IN_SET(scheduleNode->weekDays,WEEKDAY_SUN)) { String_joinCString(names,"Sun",','); }

      String_append(line,names);
      String_appendChar(line,' ');

      String_delete(names);
    }

    if (scheduleNode->hour != SCHEDULE_ANY)
    {
      String_format(line,"%d",scheduleNode->hour);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,':');
    if (scheduleNode->minute != SCHEDULE_ANY)
    {
      String_format(line,"%d",scheduleNode->minute);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,' ');
    String_format(line,"%y",scheduleNode->enabled);
    String_appendChar(line,' ');
    switch (scheduleNode->archiveType)
    {
      case ARCHIVE_TYPE_NORMAL:
        String_appendCString(line,"*");
        break;
      case ARCHIVE_TYPE_FULL:
        String_appendCString(line,"FULL");
        break;
      case ARCHIVE_TYPE_INCREMENTAL:
        String_appendCString(line,"INCREMENTAL");
        break;
      case ARCHIVE_TYPE_DIFFERENTIAL:
        String_appendCString(line,"DIFFERENTIAL");
        break;
      case ARCHIVE_TYPE_UNKNOWN:
        return FALSE;
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }

    (*formatUserData) = scheduleNode->next;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *archiveTypeText(ArchiveTypes archiveType)
{
  switch (archiveType)
  {
    case ARCHIVE_TYPE_NORMAL:       return "normal";       break;
    case ARCHIVE_TYPE_FULL:         return "full";         break;
    case ARCHIVE_TYPE_INCREMENTAL:  return "incremental";  break;
    case ARCHIVE_TYPE_DIFFERENTIAL: return "differential"; break;
    default:                        return "unknown";      break;
  }
}

bool readJobFile(const String      fileName,
                 const ConfigValue *configValues,
                 uint              configValueCount,
                 void              *configData
                )
{
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     name,value;
  long       nextIndex;

  assert(fileName != NULL);
  assert(configValues != NULL);

  // initialize variables
  line = String_new();

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open job file '%s' (error: %s)!\n",
               String_cString(fileName),
               Error_getText(error)
              );
    String_delete(line);
    return FALSE;
  }

  // parse file
  failFlag = FALSE;
  lineNb   = 0;
  name     = String_new();
  value    = String_new();
  while (!File_eof(&fileHandle) && !failFlag)
  {
    // read line
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      printError("Cannot read file '%s' (error: %s)!\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      failFlag = TRUE;
      break;
    }
    String_trim(line,STRING_WHITE_SPACES);
    lineNb++;

    // skip comments, empty lines
    if (String_isEmpty(line) || String_startsWithChar(line,'#'))
    {
      continue;
    }

    // parse line
    if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
      String_unquote(value,STRING_QUOTES);
      if (!ConfigValue_parse(String_cString(name),
                             String_cString(value),
                             configValues,configValueCount,
                             NULL,
                             NULL,
                             configData
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
      printError("Error in %s, line %ld: '%s' - skipped\n",
                 String_cString(fileName),
                 lineNb,
                 String_cString(line)
                );
    }
  }
  String_delete(value);
  String_delete(name);

  // close file
  File_close(&fileHandle);

  // free resources
  String_delete(line);

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

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  Errors         error;
  String         fileName;
  bool           printInfoFlag;
  DatabaseHandle databaseHandle;

  // init
  error = initAll();
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: Cannot initialize program resources (error: %s)\n",Error_getText(error));
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_INIT_FAIL;
  }
  globalOptions.barExecutable = argv[0];

  // parse command line: pre-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,
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
    return EXITCODE_INVALID_ARGUMENT;
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
      return EXITCODE_CONFIG_ERROR;
    }
  }
  String_delete(fileName);

  // read job file
  if (jobName != NULL)
  {
    fileName = String_new();

    // read job file
    File_setFileNameCString(fileName,serverJobsDirectory);
    File_appendFileName(fileName,jobName);
    if (!readJobFile(fileName,
                     CONFIG_VALUES,
                     SIZE_OF_ARRAY(CONFIG_VALUES),
                     NULL
                    )
       )
    {
      String_delete(fileName);
      return EXITCODE_CONFIG_ERROR;
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
    return EXITCODE_INVALID_ARGUMENT;
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
    return EXITCODE_OK;
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
    return EXITCODE_OK;
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
    return EXITCODE_FAIL;
  }

  // add delta sources
  error = Source_addSourceList(&deltaSourcePatternList);
  if (error != ERROR_NONE)
  {
    printError("Cannot add delta sources (error: %s)!\n",Error_getText(error));
    doneAll();
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_FAIL;
  }

  if (indexDatabaseFileName != NULL)
  {
    // open index database
    if (printInfoFlag) printf("Opening index database '%s'...",indexDatabaseFileName);
    error = Index_init(&databaseHandle,indexDatabaseFileName);
    if (error != ERROR_NONE)
    {
      if (printInfoFlag) printf("fail!\n");
      printError("Cannot open index database '%s' (error: %s)!\n",
                 indexDatabaseFileName,
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
      return EXITCODE_FAIL;
    }

    indexDatabaseHandle = &databaseHandle;

    if (printInfoFlag) printf("ok\n");
  }
  else
  {
    // no index database
    indexDatabaseHandle = NULL;
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
    return EXITCODE_FAIL;
  }

  error = ERROR_NONE;
  if      (daemonFlag)
  {
    // create session log file
    File_setFileName(tmpLogFileName,tmpDirectory);
    File_appendFileNameCString(tmpLogFileName,"log.txt");
    tmpLogFile = fopen(String_cString(tmpLogFileName),"w");

    // open log file
    if (logFileName != NULL)
    {
      logFile = fopen(logFileName,"a");
      if (logFile == NULL) printWarning("Cannot open log file '%s' (error: %s)!\n",logFileName,strerror(errno));
    }

    // daemon mode -> run server with network
    globalOptions.runMode = RUN_MODE_SERVER;

    if (!noDetachFlag)
    {
      // run server (detached)
      #if   defined(PLATFORM_LINUX)
        if (daemon(1,0) == 0)
        {
          // create pid file
          error = createPIDFile();
          if (error == ERROR_NONE)
          {
            // run server
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

          // close log files
          if (logFile != NULL) fclose(logFile);
          fclose(tmpLogFile); (void)unlink(String_cString(tmpLogFileName));
          File_delete(tmpLogFileName,FALSE);

          // free resources
          CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
          doneAll();
          #ifndef NDEBUG
            debugResourceDone();
            File_debugDone();
            Array_debugDone();
            String_debugDone();
            List_debugDone();
          #endif /* not NDEBUG */

          switch (error)
          {
            case ERROR_NONE:
              return EXITCODE_OK;
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
    }

    // close log files
    if (logFile != NULL) fclose(logFile);
    fclose(tmpLogFile);
    unlink(String_cString(tmpLogFileName));
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
    error = Command_create(storageName,
                           &includeEntryList,
                           &excludePatternList,
                           &compressExcludePatternList,
                           &jobOptions,
                           ARCHIVE_TYPE_NORMAL,
                           CALLBACK(inputCryptPassword,NULL),
                           CALLBACK(NULL,NULL),
                           CALLBACK(NULL,NULL),
                           NULL,
                           NULL,
                           NULL
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

          // get archive file name
          if (argc <= 1)
          {
            printError("No archive file name given!\n");
            error = ERROR_INVALID_ARGUMENT;
            break;
          }
          storageName = String_newCString(argv[1]);

          // get include patterns
          switch (command)
          {
            case COMMAND_CREATE_FILES:  entryType = ENTRY_TYPE_FILE;  break;
            case COMMAND_CREATE_IMAGES: entryType = ENTRY_TYPE_IMAGE; break;
            default:                    entryType = ENTRY_TYPE_FILE;  break;
          }
          for (z = 2; z < argc; z++)
          {
            error = EntryList_appendCString(&includeEntryList,entryType,argv[z],jobOptions.patternType);
          }

          // create archive
          error = Command_create(storageName,
                                 &includeEntryList,
                                 &excludePatternList,
                                 &compressExcludePatternList,
                                 &jobOptions,
                                 ARCHIVE_TYPE_NORMAL,
                                 CALLBACK(inputCryptPassword,NULL),
                                 CALLBACK(NULL,NULL),
                                 CALLBACK(NULL,NULL),
                                 NULL,
                                 NULL,
                                 NULL
                                );

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
                                   inputCryptPassword,
                                   NULL
                                  );
              break;
            case COMMAND_TEST:
              error = Command_test(&fileNameList,
                                   &includeEntryList,
                                   &excludePatternList,
                                   &jobOptions,
                                   inputCryptPassword,
                                   NULL
                                  );
              break;
            case COMMAND_COMPARE:
              error = Command_compare(&fileNameList,
                                      &includeEntryList,
                                      &excludePatternList,
                                      &jobOptions,
                                      inputCryptPassword,
                                      NULL
                                     );
              break;
            case COMMAND_RESTORE:
              error = Command_restore(&fileNameList,
                                      &includeEntryList,
                                      &excludePatternList,
                                      &jobOptions,
                                      inputCryptPassword,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL
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
          Crypt_initKey(&publicKey);
          Crypt_initKey(&privateKey);
          error = Crypt_createKeys(&publicKey,&privateKey,keyBits);
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
#if 0
{
  char s[200],c[2000],t[200];
  ulong n;

  strcpy(s,"Hello World");

  Crypt_keyEncrypt(&publicKey,s,200,c,2000,&n);

  Crypt_keyDecrypt(&privateKey,c,n,t,200,&n);

fprintf(stderr,"%s,%d: t=%s\n",__FILE__,__LINE__,t);

}
#endif /* 0 */
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
#warning remove
//  File_delete(tmpDirectory,TRUE);

  // close index database (if open)
  if (indexDatabaseHandle != NULL)
  {
    Database_close(indexDatabaseHandle);
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

  switch (error)
  {
    case ERROR_NONE:
      return EXITCODE_OK;
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

#ifdef __cplusplus
  }
#endif

/* end of file */
