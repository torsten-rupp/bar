/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/bar.c,v $
* $Revision: 1.12 $
* $Author: torsten $
* Contents: Backup ARchiver main program
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "cmdoptions.h"
#include "configvalues.h"
#include "strings.h"
#include "stringlists.h"
#include "arrays.h"

#include "errors.h"
#include "files.h"
#include "patternlists.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "archive.h"
#include "network.h"
#include "storage.h"
#include "misc.h"

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

#define DEFAULT_CONFIG_FILE_NAME       "bar.cfg"
#define DEFAULT_TMP_DIRECTORY          "/tmp"
#define DEFAULT_COMPRESS_MIN_FILE_SIZE 32
#define DEFAULT_SERVER_PORT            38523
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
#define DEFAULT_JOBS_DIRECTORY         CONFIG_DIR "/jobs"
#define DEFAULT_DEVICE_NAME            "/dev/dvd"

#define DEFAULT_REMOTE_BAR_EXECUTABLE "/usr/local/bin/bar"

#define DVD_UNLOAD_VOLUME_COMMAND  "eject -r %device"
#define DVD_LOAD_VOLUME_COMMAND    "eject -t %device"
#define DVD_WRITE_COMMAND          "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %file"
#define DVD_IMAGE_COMMAND          "nice mkisofs -V Backup -volset %number -r -o %image %file"
#define DVD_ECC_COMMAND            "nice dvdisaster -mRS02 -n dvd -c -i %image -v"
#define DVD_WRITE_IMAGE_COMMAND    "nice growisofs -Z %device=%image -use-the-force-luke=dao:%sectors -use-the-force-luke=noload"

#define MIN_PASSWORD_QUALITY_LEVEL 0.6

/***************************** Datatypes *******************************/

typedef enum
{
  COMMAND_NONE,

  COMMAND_CREATE,
  COMMAND_LIST,
  COMMAND_TEST,
  COMMAND_COMPARE,
  COMMAND_RESTORE,

  COMMAND_GENERATE_KEYS,
  COMMAND_NEW_KEY_PASSWORD,

  COMMAND_UNKNOWN,
} Commands;

/***************************** Variables *******************************/
GlobalOptions       globalOptions;
String              tmpDirectory;

LOCAL Commands      command;

LOCAL String        keyFileName;
LOCAL uint          keyBits;

LOCAL JobOptions    jobOptions;
LOCAL const char    *archiveFileName;
LOCAL PatternList   includePatternList;
LOCAL PatternList   excludePatternList;
LOCAL FTPServer     defaultFTPServer;
LOCAL SSHServer     defaultSSHServer;
LOCAL FTPServerList ftpServerList;
LOCAL SSHServerList sshServerList;
LOCAL Device        defaultDevice;
LOCAL DeviceList    deviceList;
LOCAL FTPServer     *currentFTPServer = &defaultFTPServer;
LOCAL SSHServer     *currentSSHServer = &defaultSSHServer;
LOCAL Device        *currentDevice = &defaultDevice;
LOCAL bool          daemonFlag;
LOCAL bool          noDetachFlag;
LOCAL uint          serverPort;
LOCAL bool          serverTLSPort;
LOCAL const char    *serverCAFileName;
LOCAL const char    *serverCertFileName;
LOCAL const char    *serverKeyFileName;
LOCAL Password      *serverPassword;
LOCAL const char    *serverJobsDirectory;

LOCAL ulong         logTypes;
LOCAL const char    *logFileName;
LOCAL const char    *logPostCommand;

LOCAL bool          batchFlag;
LOCAL bool          versionFlag;
LOCAL bool          helpFlag,xhelpFlag,helpInternalFlag;

LOCAL const char    *pidFileName;

/*---------------------------------------------------------------------*/

LOCAL StringList    configFileNameList;

LOCAL String        tmpLogFileName;
LOCAL FILE          *logFile = NULL;
LOCAL FILE          *tmpLogFile = NULL;

LOCAL String        outputLine;
LOCAL bool          outputNewLineFlag;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL bool cmdOptionParseString(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParseConfigFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParseIncludeExclude(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);

LOCAL const CommandLineUnit COMMAND_LINE_BYTES_UNITS[] =
{
  {"G",1024*1024*1024},
  {"M",1024*1024},
  {"K",1024},
};

LOCAL const CommandLineUnit COMMAND_LINE_BITS_UNITS[] =
{
  {"K",1024},
};

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_PATTERN_TYPES[] =
{
  {"glob",    PATTERN_TYPE_GLOB,          "glob patterns: * and ?"                      },
  {"regex",   PATTERN_TYPE_REGEX,         "regular expression pattern matching"         },
  {"extended",PATTERN_TYPE_EXTENDED_REGEX,"extended regular expression pattern matching"},
};

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
};

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS[] =
{
  {"none",      CRYPT_ALGORITHM_NONE,      "no crypting"          },

  #ifdef HAVE_GCRYPT
    {"3DES",      CRYPT_ALGORITHM_3DES,      "3DES cipher"          },
    {"CAST5",     CRYPT_ALGORITHM_CAST5,     "CAST5 cipher"         },
    {"BLOWFISH",  CRYPT_ALGORITHM_BLOWFISH,  "Blowfish cipher"      },
    {"AES128",    CRYPT_ALGORITHM_AES128,    "AES cipher 128bit"    },
    {"AES192",    CRYPT_ALGORITHM_AES192,    "AES cipher 192bit"    },
    {"AES256",    CRYPT_ALGORITHM_AES256,    "AES cipher 256bit"    },
    {"TWOFISH128",CRYPT_ALGORITHM_TWOFISH128,"Twofish cipher 128bit"},
    {"TWOFISH256",CRYPT_ALGORITHM_TWOFISH256,"Twofish cipher 256bit"},
  #endif /* HAVE_GCRYPT */
};

LOCAL const CommandLineOptionSet COMMAND_LINE_OPTIONS_LOG_TYPES[] =
{
  {"none",      LOG_TYPE_NONE,              "no logging"               },
  {"errors",    LOG_TYPE_ERROR,             "log errors"               },
  {"warnings",  LOG_TYPE_WARNING,           "log warningss"            },

  {"ok",        LOG_TYPE_FILE_OK,           "log stored/restored files"},
  {"unknown",   LOG_TYPE_FILE_TYPE_UNKNOWN, "log unknown files"        },
  {"skipped",   LOG_TYPE_FILE_ACCESS_DENIED,"log skipped files"        },
  {"missing",   LOG_TYPE_FILE_MISSING,      "log missing files"        },
  {"incomplete",LOG_TYPE_FILE_INCOMPLETE,   "log incomplete files"     },
  {"excluded",  LOG_TYPE_FILE_EXCLUDED,     "log excluded files"       },

  {"storage",   LOG_TYPE_STORAGE,           "log storage"              },

  {"all",       LOG_TYPE_ALL,               "log everything"           },
};

LOCAL const CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM         ("create",                       'c',0,0,command,                                   COMMAND_LIST,COMMAND_CREATE,                                       "create new archive"                                                       ),
  CMD_OPTION_ENUM         ("list",                         'l',0,0,command,                                   COMMAND_LIST,COMMAND_LIST,                                         "list contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("test",                         't',0,0,command,                                   COMMAND_LIST,COMMAND_TEST,                                         "test contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("compare",                      'd',0,0,command,                                   COMMAND_LIST,COMMAND_COMPARE,                                      "compare contents of archive with files"                                   ),
  CMD_OPTION_ENUM         ("extract",                      'x',0,0,command,                                   COMMAND_LIST,COMMAND_RESTORE,                                      "restore archive"                                                          ),
  CMD_OPTION_ENUM         ("generate-keys",                0,  0,0,command,                                   COMMAND_LIST,COMMAND_GENERATE_KEYS,                                "generate new public/private key pair"                                     ),
//  CMD_OPTION_ENUM         ("new-key-password",             0,  0,0,command,                                   COMMAND_LIST,COMMAND_NEW_KEY_PASSWORD,                           "set new private key password"                                             ),
  CMD_OPTION_INTEGER      ("generate-keys-bits",           0,  1,0,keyBits,                                   DEFAULT_ASYMMETRIC_CRYPT_KEY_BITS,
                                                                                                              MIN_ASYMMETRIC_CRYPT_KEY_BITS,
                                                                                                              MAX_ASYMMETRIC_CRYPT_KEY_BITS,COMMAND_LINE_BITS_UNITS,             "key bits"                                                                 ),

  CMD_OPTION_SPECIAL      ("config",                       0,  1,0,NULL,                                      NULL,cmdOptionParseConfigFile,NULL,                                "configuration file","file name"                                           ),

  CMD_OPTION_INTEGER64    ("archive-part-size",            's',0,0,jobOptions.archivePartSize,                0,0,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                        "approximated archive part size"                                           ),
  CMD_OPTION_SPECIAL      ("tmp-directory",                0,  1,0,&globalOptions.tmpDirectory,               DEFAULT_TMP_DIRECTORY,cmdOptionParseString,NULL,                   "temporary directory","path"                                               ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",                 0,  1,0,globalOptions.maxTmpSize,                  0,0,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                        "max. size of temporary files"                                             ),

  CMD_OPTION_ENUM         ("full",                         'f',0,0,jobOptions.archiveType,                    ARCHIVE_TYPE_NORMAL,ARCHIVE_TYPE_FULL,                             "create full archive and incremental list file"                            ),
  CMD_OPTION_ENUM         ("incremental",                  'i',0,0,jobOptions.archiveType,                    ARCHIVE_TYPE_NORMAL,ARCHIVE_TYPE_INCREMENTAL,                      "create incremental archive"                                               ),
  CMD_OPTION_SPECIAL      ("incremental-list-file",        'I',1,0,&jobOptions.incrementalListFileName,       NULL,cmdOptionParseString,NULL,                                    "incremental list file name (default: <archive name>.bid)","file name"     ),

  CMD_OPTION_INTEGER      ("directory-strip",              'p',1,0,jobOptions.directoryStripCount,            0,0,INT_MAX,NULL,                                                  "number of directories to strip on extract"                                ),
  CMD_OPTION_SPECIAL      ("directory",                    0,  0,0,&jobOptions.directory   ,                  NULL,cmdOptionParseString,NULL,                                    "directory to restore files","path"                                        ),
  CMD_OPTION_INTEGER      ("nice-level",                   0,  1,0,globalOptions.niceLevel,                   0,0,19,NULL,                                                       "general nice level of processes/threads"                                  ),

  CMD_OPTION_INTEGER      ("max-band-width",               0,  1,0,globalOptions.maxBandWidth,                0,0,INT_MAX,COMMAND_LINE_BITS_UNITS,                               "max. network band width to use"                                           ),

  CMD_OPTION_SELECT       ("pattern-type",                 0,  1,0,jobOptions.patternType,                    PATTERN_TYPE_GLOB,COMMAND_LINE_OPTIONS_PATTERN_TYPES,              "select pattern type"                                                      ),

  CMD_OPTION_SPECIAL      ("include",                      '#',0,1,&includePatternList,                       NULL,cmdOptionParseIncludeExclude,NULL,                            "include pattern","pattern"                                                ),
  CMD_OPTION_SPECIAL      ("exclude",                      '!',0,1,&excludePatternList,                       NULL,cmdOptionParseIncludeExclude,NULL,                            "exclude pattern","pattern"                                                ),

  CMD_OPTION_SELECT       ("compress-algorithm",           'z',0,0,jobOptions.compressAlgorithm,              COMPRESS_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHMS,  "select compress algorithm to use"                                         ),
  CMD_OPTION_INTEGER      ("compress-min-size",            0,  1,0,globalOptions.compressMinFileSize,         DEFAULT_COMPRESS_MIN_FILE_SIZE,0,INT_MAX,COMMAND_LINE_BYTES_UNITS, "minimal size of file for compression"                                     ),

  CMD_OPTION_SELECT       ("crypt-algorithm",              'y',0,0,jobOptions.cryptAlgorithm,                 CRYPT_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS,        "select crypt algorithm to use"                                            ),
  CMD_OPTION_ENUM         ("crypt-asymmetric",             'a',0,0,jobOptions.cryptType,                      CRYPT_TYPE_SYMMETRIC,CRYPT_TYPE_ASYMMETRIC,                        "use asymetric encryption"                                                 ),
  CMD_OPTION_SPECIAL      ("crypt-password",               0,  0,0,&globalOptions.cryptPassword,              NULL,cmdOptionParsePassword,NULL,                                  "crypt password (use with care!)","password"                               ),
  CMD_OPTION_SPECIAL      ("crypt-public-key",             0,  0,0,&jobOptions.cryptPublicKeyFileName,        NULL,cmdOptionParseString,NULL,                                    "public key for encryption","file name"                                    ),
  CMD_OPTION_SPECIAL      ("crypt-private-key",            0,  0,0,&jobOptions.cryptPrivateKeyFileName,       NULL,cmdOptionParseString,NULL,                                    "private key for decryption","file name"                                   ),

  CMD_OPTION_SPECIAL      ("ftp-login-name",               0,  0,0,&defaultFTPServer.loginName,               NULL,cmdOptionParseString,NULL,                                    "ftp login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ftp-password",                 0,  0,0,&defaultFTPServer.password,                NULL,cmdOptionParsePassword,NULL,                                  "ftp password (use with care!)","password"                                 ),

  CMD_OPTION_INTEGER      ("ssh-port",                     0,  0,0,defaultSSHServer.port,                     22,0,65535,NULL,                                                   "ssh port (default: 22)"                                                   ),
  CMD_OPTION_SPECIAL      ("ssh-login-name",               0,  0,0,&defaultSSHServer.loginName,               NULL,cmdOptionParseString,NULL,                                    "ssh login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ssh-password",                 0,  0,0,&defaultSSHServer.password,                NULL,cmdOptionParsePassword,NULL,                                  "ssh password (use with care!)","password"                                 ),
  CMD_OPTION_SPECIAL      ("ssh-public-key",               0,  1,0,&defaultSSHServer.publicKeyFileName,       NULL,cmdOptionParseString,NULL,                                    "ssh public key file name","file name"                                     ),
  CMD_OPTION_SPECIAL      ("ssh-private-key",              0,  1,0,&defaultSSHServer.privateKeyFileName,      NULL,cmdOptionParseString,NULL,                                    "ssh privat key file name","file name"                                     ),

  CMD_OPTION_BOOLEAN      ("daemon",                       0,  1,0,daemonFlag,                                FALSE,                                                             "run in daemon mode"                                                       ),
  CMD_OPTION_BOOLEAN      ("no-detach",                    'D',1,0,noDetachFlag,                              FALSE,                                                             "do not detach in daemon mode"                                             ),
  CMD_OPTION_INTEGER      ("server-port",                  0,  1,0,serverPort,                                DEFAULT_SERVER_PORT,0,65535,NULL,                                  "server port"                                                              ),
  CMD_OPTION_INTEGER      ("server-tls-port",              0,  1,0,serverTLSPort,                             DEFAULT_TLS_SERVER_PORT,0,65535,NULL,                              "TLS (SSL) server port"                                                    ),
  CMD_OPTION_STRING       ("server-ca-file",               0,  1,0,serverCAFileName,                          DEFAULT_TLS_SERVER_CA_FILE,                                        "TLS (SSL) server certificate authority file (CA file)","file name"        ),
  CMD_OPTION_STRING       ("server-cert-file",             0,  1,0,serverCertFileName,                        DEFAULT_TLS_SERVER_CERTIFICATE_FILE,                               "TLS (SSL) server certificate file","file name"                            ),
  CMD_OPTION_STRING       ("server-key-file",              0,  1,0,serverKeyFileName,                         DEFAULT_TLS_SERVER_KEY_FILE,                                       "TLS (SSL) server key file","file name"                                    ),
  CMD_OPTION_SPECIAL      ("server-password",              0,  1,0,&serverPassword,                           NULL,cmdOptionParsePassword,NULL,                                  "server password (use with care!)","password"                              ),
  CMD_OPTION_STRING       ("server-jobs-directory",        0,  1,0,serverJobsDirectory,                       DEFAULT_JOBS_DIRECTORY,                                             "server job directory","path name"                                        ),

  CMD_OPTION_BOOLEAN      ("batch",                        0,  2,0,batchFlag,                                 FALSE,                                                             "run in batch mode"                                                        ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",        0,  1,0,&globalOptions.remoteBARExecutable,        DEFAULT_REMOTE_BAR_EXECUTABLE,cmdOptionParseString,NULL,           "remote BAR executable","file name"                                        ),

  CMD_OPTION_SPECIAL      ("dvd-request-volume-command",   0,  1,0,&globalOptions.dvd.requestVolumeCommand,   NULL,cmdOptionParseString,NULL,                                    "request new DVD volume command","command"                                 ),
  CMD_OPTION_SPECIAL      ("dvd-unload-volume-command",    0,  1,0,&globalOptions.dvd.unloadVolumeCommand,    DVD_UNLOAD_VOLUME_COMMAND,cmdOptionParseString,NULL,               "unload DVD volume command","command"                                      ),
  CMD_OPTION_SPECIAL      ("dvd-load-volume-command",      0,  1,0,&globalOptions.dvd.loadVolumeCommand,      DVD_LOAD_VOLUME_COMMAND,cmdOptionParseString,NULL,                 "load DVD volume command","command"                                        ),
  CMD_OPTION_INTEGER64    ("dvd-volume-size",              0,  1,0,globalOptions.dvd.volumeSize,              0LL,0LL,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                    "DVD volume size"                                                          ),
  CMD_OPTION_SPECIAL      ("dvd-image-pre-command",        0,  1,0,&globalOptions.dvd.imagePreProcessCommand, NULL,cmdOptionParseString,NULL,                                    "make DVD image pre-process command","command"                             ),
  CMD_OPTION_SPECIAL      ("dvd-image-post-command",       0,  1,0,&globalOptions.dvd.imagePostProcessCommand,NULL,cmdOptionParseString,NULL,                                    "make DVD image post-process command","command"                            ),
  CMD_OPTION_SPECIAL      ("dvd-image-command",            0,  1,0,&globalOptions.dvd.imageCommand,           DVD_IMAGE_COMMAND,cmdOptionParseString,NULL,                       "make DVD image command","command"                                         ),
  CMD_OPTION_SPECIAL      ("dvd-ecc-pre-command",          0,  1,0,&globalOptions.dvd.eccPreProcessCommand,   NULL,cmdOptionParseString,NULL,                                    "make DVD error-correction codes pre-process command","command"            ),
  CMD_OPTION_SPECIAL      ("dvd-ecc-post-command",         0,  1,0,&globalOptions.dvd.eccPostProcessCommand,  NULL,cmdOptionParseString,NULL,                                    "make DVD error-correction codes post-process command","command"           ),
  CMD_OPTION_SPECIAL      ("dvd-ecc-command",              0,  1,0,&globalOptions.dvd.eccCommand,             DVD_ECC_COMMAND,cmdOptionParseString,NULL,                         "make DVD error-correction codes command","command"                        ),
  CMD_OPTION_SPECIAL      ("dvd-write-pre-command",        0,  1,0,&globalOptions.dvd.writePreProcessCommand, NULL,cmdOptionParseString,NULL,                                    "write DVD pre-process command","command"                                  ),
  CMD_OPTION_SPECIAL      ("dvd-write-post-command",       0,  1,0,&globalOptions.dvd.writePostProcessCommand,NULL,cmdOptionParseString,NULL,                                    "write DVD post-process command","command"                                 ),
  CMD_OPTION_SPECIAL      ("dvd-write-command",            0,  1,0,&globalOptions.dvd.writeCommand,           DVD_WRITE_COMMAND,cmdOptionParseString,NULL,                       "write DVD command","command"                                              ),
  CMD_OPTION_SPECIAL      ("dvd-write-command",            0,  1,0,&globalOptions.dvd.writeImageCommand,      DVD_WRITE_IMAGE_COMMAND,cmdOptionParseString,NULL,                 "write DVD image command","command"                                        ),

  CMD_OPTION_SPECIAL      ("device",                       0,  1,0,&globalOptions.defaultDeviceName,          DEFAULT_DEVICE_NAME,cmdOptionParseString,NULL,                     "default device","device name"                                             ),
  CMD_OPTION_SPECIAL      ("device-request-volume-command",0,  1,0,&defaultDevice.requestVolumeCommand,       NULL,cmdOptionParseString,NULL,                                    "request new volume command","command"                                     ),
  CMD_OPTION_SPECIAL      ("device-load-volume-command",   0,  1,0,&defaultDevice.loadVolumeCommand,          NULL,cmdOptionParseString,NULL,                                    "load volume command","command"                                            ),
  CMD_OPTION_SPECIAL      ("device-unload-volume-command", 0,  1,0,&defaultDevice.unloadVolumeCommand,        NULL,cmdOptionParseString,NULL,                                    "unload volume command","command"                                          ),
  CMD_OPTION_INTEGER64    ("device-volume-size",           0,  1,0,defaultDevice.volumeSize,                  0LL,0LL,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                    "volume size"                                                              ),
  CMD_OPTION_SPECIAL      ("device-image-pre-command",     0,  1,0,&defaultDevice.imagePreProcessCommand,     NULL,cmdOptionParseString,NULL,                                    "make image pre-process command","command"                                 ),
  CMD_OPTION_SPECIAL      ("device-image-post-command",    0,  1,0,&defaultDevice.imagePostProcessCommand,    NULL,cmdOptionParseString,NULL,                                    "make image post-process command","command"                                ),
  CMD_OPTION_SPECIAL      ("device-image-command",         0,  1,0,&defaultDevice.imageCommand,               NULL,cmdOptionParseString,NULL,                                    "make image command","command"                                             ),
  CMD_OPTION_SPECIAL      ("device-ecc-pre-command",       0,  1,0,&defaultDevice.eccPreProcessCommand,       NULL,cmdOptionParseString,NULL,                                    "make error-correction codes pre-process command","command"                ),
  CMD_OPTION_SPECIAL      ("device-ecc-post-command",      0,  1,0,&defaultDevice.eccPostProcessCommand,      NULL,cmdOptionParseString,NULL,                                    "make error-correction codes post-process command","command"               ),
  CMD_OPTION_SPECIAL      ("device-ecc-command",           0,  1,0,&defaultDevice.eccCommand,                 NULL,cmdOptionParseString,NULL,                                    "make error-correction codes command","command"                            ),
  CMD_OPTION_SPECIAL      ("device-write-pre-command",     0,  1,0,&defaultDevice.writePreProcessCommand,     NULL,cmdOptionParseString,NULL,                                    "write device pre-process command","command"                               ),
  CMD_OPTION_SPECIAL      ("device-write-post-command",    0,  1,0,&defaultDevice.writePostProcessCommand,    NULL,cmdOptionParseString,NULL,                                    "write device post-process command","command"                              ),
  CMD_OPTION_SPECIAL      ("device-write-command",         0,  1,0,&defaultDevice.writeCommand,               NULL,cmdOptionParseString,NULL,                                    "write device command","command"                                           ),

  CMD_OPTION_BOOLEAN      ("ecc",                          0,  1,0,jobOptions.errorCorrectionCodesFlag,       FALSE,                                                             "add error-correction codes with 'dvdisaster' tool"                        ),

  CMD_OPTION_SET          ("log",                          0,  1,0,logTypes,                                  0,COMMAND_LINE_OPTIONS_LOG_TYPES,                                  "log types"                                                                ),
  CMD_OPTION_STRING       ("log-file",                     0,  1,0,logFileName,                               NULL,                                                              "log file name","file name"                                                ),
  CMD_OPTION_STRING       ("log-post-command",             0,  1,0,logPostCommand,                            NULL,                                                              "log file post-process command","command"                                  ),

  CMD_OPTION_STRING       ("pid-file",                     0,  1,0,pidFileName,                               NULL,                                                              "process id file name","file name"                                         ),

  CMD_OPTION_BOOLEAN      ("group",                        'g',0,0,globalOptions.groupFlag,                   FALSE,                                                             "group files in list"                                                      ),
  CMD_OPTION_BOOLEAN      ("all",                          0,  0,0,globalOptions.allFlag,                     FALSE,                                                             "show all files"                                                           ),
  CMD_OPTION_BOOLEAN      ("long-format",                  0,  0,0,globalOptions.longFormatFlag,              FALSE,                                                             "list in long format"                                                      ),
  CMD_OPTION_BOOLEAN      ("no-header-footer",             0,  0,0,globalOptions.noHeaderFooterFlag,          FALSE,                                                             "output no header/footer in list"                                          ),
  CMD_OPTION_BOOLEAN      ("delete-old-archive-files",     0,  1,0,globalOptions.deleteOldArchiveFilesFlag,   FALSE,                                                             "delete old archive files after creating new files"                        ),

  CMD_OPTION_BOOLEAN      ("skip-unreadable",              0,  0,0,jobOptions.skipUnreadableFlag,             TRUE,                                                              "skip unreadable files"                                                    ),
  CMD_OPTION_BOOLEAN      ("overwrite-archive-files",      0,  0,0,jobOptions.overwriteArchiveFilesFlag,      FALSE,                                                             "overwrite existing archive files"                                         ),
  CMD_OPTION_BOOLEAN      ("overwrite-files",              0,  0,0,jobOptions.overwriteFilesFlag,             FALSE,                                                             "overwrite existing files"                                                 ),
  CMD_OPTION_BOOLEAN      ("wait-first-volume",            0,  1,0,jobOptions.waitFirstVolumeFlag,            FALSE,                                                             "wait for first volume"                                                    ),
  CMD_OPTION_BOOLEAN      ("no-storage",                   0,  1,0,jobOptions.noStorageFlag,                  FALSE,                                                             "do not store archives (skip storage)"                                     ),
  CMD_OPTION_BOOLEAN      ("no-bar-on-dvd",                0,  1,0,jobOptions.noBAROnDVDFlag,                 FALSE,                                                             "do not store a copy of BAR on DVDs"                                       ),
  CMD_OPTION_BOOLEAN      ("stop-on-error",                0,  1,0,jobOptions.stopOnErrorFlag,                FALSE,                                                             "immediately stop on error"                                                ),
  CMD_OPTION_BOOLEAN      ("no-default-config",            0,  1,0,globalOptions.noDefaultConfigFlag,         FALSE,                                                             "do not read personal config file ~/.bar/" DEFAULT_CONFIG_FILE_NAME        ),
  CMD_OPTION_BOOLEAN      ("quiet",                        0,  1,0,globalOptions.quietFlag,                   FALSE,                                                             "surpress any output"                                                      ),
  CMD_OPTION_INTEGER_RANGE("verbose",                      'v',1,0,globalOptions.verboseLevel,                1,0,3,NULL,                                                        "verbosity level"                                                          ),

  CMD_OPTION_BOOLEAN      ("version",                      0  ,0,0,versionFlag,                               FALSE,                                                             "output version"                                                           ),
  CMD_OPTION_BOOLEAN      ("help",                         'h',0,0,helpFlag,                                  FALSE,                                                             "output this help"                                                         ),
  CMD_OPTION_BOOLEAN      ("xhelp",                        0,  0,0,xhelpFlag,                                 FALSE,                                                             "output help to extended options"                                          ),
  CMD_OPTION_BOOLEAN      ("help-internal",                0,  1,0,helpInternalFlag,                          FALSE,                                                             "output help to internal options"                                          ),
};

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value);

LOCAL const ConfigValueUnit CONFIG_VALUE_BYTES_UNITS[] =
{
  {"K",1024},
  {"M",1024*1024},
  {"G",1024*1024*1024},
};

LOCAL const ConfigValueUnit CONFIG_VALUE_BITS_UNITS[] =
{
  {"K",1024},
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
  #endif /* HAVE_GCRYPT */
};

LOCAL const ConfigValueSelect CONFIG_VALUE_CRYPT_TYPES[] =
{
  #ifdef HAVE_GCRYPT
    {"symmetric", CRYPT_TYPE_SYMMETRIC },
    {"asymmetric",CRYPT_TYPE_ASYMMETRIC},
  #endif /* HAVE_GCRYPT */
};

LOCAL const ConfigValueSet CONFIG_VALUE_LOG_TYPES[] =
{
  {"none",      LOG_TYPE_NONE              },
  {"errors",    LOG_TYPE_ERROR             },
  {"warnings",  LOG_TYPE_WARNING           },

  {"ok",        LOG_TYPE_FILE_OK           },
  {"unknown",   LOG_TYPE_FILE_TYPE_UNKNOWN },
  {"skipped",   LOG_TYPE_FILE_ACCESS_DENIED},
  {"missing",   LOG_TYPE_FILE_MISSING      },
  {"incomplete",LOG_TYPE_FILE_INCOMPLETE   },
  {"excluded",  LOG_TYPE_FILE_EXCLUDED     },

  {"storage",   LOG_TYPE_STORAGE           },

  {"all",       LOG_TYPE_ALL               },
};

LOCAL const ConfigValue CONFIG_VALUES[] =
{
  CONFIG_VALUE_SPECIAL  ("config",                       NULL,-1,                                                configValueParseConfigFile,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_INTEGER64("archive-part-size",            &jobOptions.archivePartSize,-1,                         0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("tmp-directory",                &globalOptions.tmpDirectory,-1                          ),
  CONFIG_VALUE_INTEGER64("max-tmp-size",                 &globalOptions.maxTmpSize,-1,                           0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER  ("directory-strip",              &jobOptions.directoryStripCount,-1,                     0,INT_MAX,NULL),
  CONFIG_VALUE_STRING   ("directory",                    &jobOptions.directory,-1                                ),
  CONFIG_VALUE_INTEGER  ("nice-level",                   &globalOptions.niceLevel,-1,                            0,19,NULL),

  CONFIG_VALUE_INTEGER  ("max-band-width",               &globalOptions.maxBandWidth,-1,                         0,INT_MAX,CONFIG_VALUE_BITS_UNITS),

  CONFIG_VALUE_SELECT   ("pattern-type",                 &jobOptions.patternType,-1,                             CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_VALUE_SPECIAL  ("include",                      &includePatternList,-1,                                 configValueParseIncludeExclude,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("exclude",                      &excludePatternList,-1,                                 configValueParseIncludeExclude,NULL,NULL,NULL,&jobOptions.patternType),

  CONFIG_VALUE_SELECT   ("compress-algorithm",           &jobOptions.compressAlgorithm,-1,                       CONFIG_VALUE_COMPRESS_ALGORITHMS),
  CONFIG_VALUE_INTEGER  ("compress-min-size",            &globalOptions.compressMinFileSize,-1,                  0,INT_MAX,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_SELECT   ("crypt-algorithm",              &jobOptions.cryptAlgorithm,-1,                          CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_VALUE_SELECT   ("crypt-type",                   &jobOptions.cryptType,-1,                               CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_VALUE_SPECIAL  ("crypt-password",               &globalOptions.cryptPassword,-1,                        configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_STRING   ("crypt-public-key",             &jobOptions.cryptPublicKeyFileName,-1                   ),
  CONFIG_VALUE_STRING   ("crypt-private-key",            &jobOptions.cryptPrivateKeyFileName,-1                  ),

  CONFIG_VALUE_STRING   ("ftp-login-name",               &currentFTPServer,offsetof(FTPServer,loginName)         ),
  CONFIG_VALUE_SPECIAL  ("ftp-password",                 &currentFTPServer,offsetof(FTPServer,password),         configValueParsePassword,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_INTEGER  ("ssh-port",                     &currentSSHServer,offsetof(SSHServer,port),             0,65535,NULL),
  CONFIG_VALUE_STRING   ("ssh-login-name",               &currentSSHServer,offsetof(SSHServer,loginName)         ),
  CONFIG_VALUE_SPECIAL  ("ssh-password",                 &currentSSHServer,offsetof(SSHServer,password),         configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_STRING   ("ssh-public-key",               &currentSSHServer,offsetof(SSHServer,publicKeyFileName) ),
  CONFIG_VALUE_STRING   ("ssh-private-key",              &currentSSHServer,offsetof(SSHServer,privateKeyFileName)),

  CONFIG_VALUE_INTEGER  ("server-port",                  &serverPort,-1,                                         0,65535,NULL),
  CONFIG_VALUE_INTEGER  ("server-tls-port",              &serverTLSPort,-1,                                      0,65535,NULL),
  CONFIG_VALUE_CSTRING  ("server-ca-file",               &serverCAFileName,-1                                    ),
  CONFIG_VALUE_CSTRING  ("server-cert-file",             &serverCertFileName,-1                                  ),
  CONFIG_VALUE_CSTRING  ("server-key-file",              &serverKeyFileName,-1                                   ),
  CONFIG_VALUE_SPECIAL  ("server-password",              &serverPassword,-1,                                     configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_CSTRING  ("server-jobs-directory",        &serverJobsDirectory,-1                                 ),

  CONFIG_VALUE_STRING   ("remote-bar-executable",        &globalOptions.remoteBARExecutable,-1                   ),

  CONFIG_VALUE_STRING   ("dvd-request-volume-command",   &globalOptions.dvd.requestVolumeCommand,-1              ),
  CONFIG_VALUE_STRING   ("dvd-unload-volume-command",    &globalOptions.dvd.unloadVolumeCommand,-1               ),
  CONFIG_VALUE_STRING   ("dvd-load-volume-command",      &globalOptions.dvd.loadVolumeCommand,-1                 ),
  CONFIG_VALUE_INTEGER64("dvd-volume-size",              &globalOptions.dvd.volumeSize,-1,                       0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("dvd-image-pre-command",        &globalOptions.dvd.imagePreProcessCommand,-1            ),
  CONFIG_VALUE_STRING   ("dvd-image-post-command",       &globalOptions.dvd.imagePostProcessCommand,-1           ),
  CONFIG_VALUE_STRING   ("dvd-image-command",            &globalOptions.dvd.imageCommand,-1                      ),
  CONFIG_VALUE_STRING   ("dvd-ecc-pre-command",          &globalOptions.dvd.eccPreProcessCommand,-1              ),
  CONFIG_VALUE_STRING   ("dvd-ecc-post-command",         &globalOptions.dvd.eccPostProcessCommand,-1             ),
  CONFIG_VALUE_STRING   ("dvd-ecc-command",              &globalOptions.dvd.eccCommand,-1                        ),
  CONFIG_VALUE_STRING   ("dvd-write-pre-command",        &globalOptions.dvd.writePreProcessCommand,-1            ),
  CONFIG_VALUE_STRING   ("dvd-write-post-command",       &globalOptions.dvd.writePostProcessCommand,-1           ),
  CONFIG_VALUE_STRING   ("dvd-write-command",            &globalOptions.dvd.writeCommand,-1                      ),
  CONFIG_VALUE_STRING   ("dvd-write-command",            &globalOptions.dvd.writeImageCommand,-1                 ),

  CONFIG_VALUE_STRING   ("device",                       &globalOptions.defaultDeviceName,-1                     ),
  CONFIG_VALUE_STRING   ("device-request-volume-command",&currentDevice,offsetof(Device,requestVolumeCommand)    ),
  CONFIG_VALUE_STRING   ("device-unload-volume-command", &currentDevice,offsetof(Device,unloadVolumeCommand)     ),
  CONFIG_VALUE_STRING   ("device-load-volume-command",   &currentDevice,offsetof(Device,loadVolumeCommand)       ),
  CONFIG_VALUE_INTEGER64("device-volume-size",           &currentDevice,offsetof(Device,volumeSize),             0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_STRING   ("device-image-pre-command",     &currentDevice,offsetof(Device,imagePreProcessCommand)  ),
  CONFIG_VALUE_STRING   ("device-image-post-command",    &currentDevice,offsetof(Device,imagePostProcessCommand) ),
  CONFIG_VALUE_STRING   ("device-image-command",         &currentDevice,offsetof(Device,imageCommand)            ),
  CONFIG_VALUE_STRING   ("device-ecc-pre-command",       &currentDevice,offsetof(Device,eccPreProcessCommand)    ),
  CONFIG_VALUE_STRING   ("device-ecc-post-command",      &currentDevice,offsetof(Device,eccPostProcessCommand)   ),
  CONFIG_VALUE_STRING   ("device-ecc-command",           &currentDevice,offsetof(Device,eccCommand)              ),
  CONFIG_VALUE_STRING   ("device-write-pre-command",     &currentDevice,offsetof(Device,writePreProcessCommand)  ),
  CONFIG_VALUE_STRING   ("device-write-post-command",    &currentDevice,offsetof(Device,writePostProcessCommand) ),
  CONFIG_VALUE_STRING   ("device-write-command",         &currentDevice,offsetof(Device,writeCommand)            ),

  CONFIG_VALUE_BOOLEAN  ("ecc",                          &jobOptions.errorCorrectionCodesFlag,-1                 ),

  CONFIG_VALUE_SET      ("log",                          &logTypes,-1,                                           CONFIG_VALUE_LOG_TYPES),
  CONFIG_VALUE_CSTRING  ("log-file",                     &logFileName,-1                                         ),
  CONFIG_VALUE_CSTRING  ("log-post-command",             &logPostCommand,-1                                      ),

  CONFIG_VALUE_CSTRING  ("pid-file",                     &pidFileName,-1                                         ),

  CONFIG_VALUE_BOOLEAN  ("skip-unreadable",              &jobOptions.skipUnreadableFlag,-1                       ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-archive-files",      &jobOptions.overwriteArchiveFilesFlag,-1                ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-files",              &jobOptions.overwriteFilesFlag,-1                       ),
  CONFIG_VALUE_BOOLEAN  ("wait-first-volume",            &jobOptions.waitFirstVolumeFlag,-1                      ),
  CONFIG_VALUE_BOOLEAN  ("no-bar-on-dvd",                &jobOptions.noBAROnDVDFlag,-1                           ),
  CONFIG_VALUE_BOOLEAN  ("quiet",                        &globalOptions.quietFlag,-1                             ),
  CONFIG_VALUE_INTEGER  ("verbose",                      &globalOptions.verboseLevel,-1,                         0,3,NULL),

//  CONFIG_VALUE_SPECIAL  ("schedule",                     &jobOptions.scheduleList,-1,                          configValueParseSchedule,NULL,NULL,NULL,NULL),
};

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : output
* Purpose: output string to console
* Input  : file            - output stream (stdout, stderr)
*          saveRestoreFlag - TRUE if current line should be saved and
*                            restored
*          string          - string
* Output : -
* Return : -
* Notes  :  if saveRestoreFlag is TRUE the current line is saved, the
*           string is printed and the line is restored
\***********************************************************************/

LOCAL void output(FILE *file, bool saveRestoreFlag, const String string)
{
  uint z;

  if (saveRestoreFlag)
  {
    /* wipe-out current line */
    for (z = 0; z < String_length(outputLine); z++)
    {
      fwrite("\b",1,1,file);
    }
    for (z = 0; z < String_length(outputLine); z++)
    {
      fwrite(" ",1,1,file);
    }
    for (z = 0; z < String_length(outputLine); z++)
    {
      fwrite("\b",1,1,file);
    }

    /* output line */
    fwrite(String_cString(string),1,String_length(string),file);

    /* restore line */
    fwrite(String_cString(outputLine),1,String_length(outputLine),file);
  }
  else
  {
    /* output string */
    fwrite(String_cString(string),1,String_length(string),file);

    /* store */
    if (String_index(string,STRING_END) == '\n')
    {
      String_clear(outputLine);
    }
    else
    {
      String_append(outputLine,string);
    }
  }
  fflush(stdout);
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
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     name,value;
  ulong      nextIndex;

  assert(fileName != NULL);

  /* open file */
  error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open file '%s' (error: %s)!\n",
               String_cString(fileName),
               Errors_getText(error)
              );
    return FALSE;
  }

  /* parse file */
  if (printInfoFlag) printf("Reading config file '%s'...",String_cString(fileName));
  failFlag   = FALSE;
  lineNb     = 0;
  line       = String_new();
  name       = String_new();
  value      = String_new();
  while (!File_eof(&fileHandle) && !failFlag)
  {
    /* read line */
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      if (printInfoFlag) printf("FAIL!\n");
      printError("Cannot read file '%s' (error: %s)!\n",
                 String_cString(fileName),
                 Errors_getText(error)
                );
      failFlag = TRUE;
      break;
    }
    lineNb++;

    /* skip comments, empty lines */
    if ((String_length(line) == 0) || (String_index(line,0) == '#'))
    {
      continue;
    }

    /* parse line */
    if      (String_parse(line,STRING_BEGIN,"[ftp-server %S]",NULL,name))
    {
      FTPServerNode *ftpServerNode;

      ftpServerNode = LIST_NEW_NODE(FTPServerNode);
      if (ftpServerNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      ftpServerNode->name                = String_duplicate(name);
      ftpServerNode->ftpServer.loginName = NULL;
      ftpServerNode->ftpServer.password  = NULL;

      List_append(&ftpServerList,ftpServerNode);

      currentFTPServer = &ftpServerNode->ftpServer;
      currentSSHServer = NULL;
    }
    else if (String_parse(line,STRING_BEGIN,"[ssh-server %S]",NULL,name))
    {
      SSHServerNode *sshServerNode;

      sshServerNode = LIST_NEW_NODE(SSHServerNode);
      if (sshServerNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      sshServerNode->name                         = String_duplicate(name);
      sshServerNode->sshServer.port               = 22;
      sshServerNode->sshServer.loginName          = NULL;
      sshServerNode->sshServer.password           = NULL;
      sshServerNode->sshServer.publicKeyFileName  = NULL;
      sshServerNode->sshServer.privateKeyFileName = NULL;

      List_append(&sshServerList,sshServerNode);

      currentFTPServer = NULL;
      currentSSHServer = &sshServerNode->sshServer;
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
      currentFTPServer = &defaultFTPServer;
      currentSSHServer = &defaultSSHServer;
      currentDevice    = &defaultDevice;
    }
    else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
//      String_unquote(String_trim(String_sub(value,line,nextIndex,STRING_END),STRING_WHITE_SPACES),STRING_QUOTES);
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
    if (printInfoFlag) printf("ok\n");
  }
  String_delete(value);
  String_delete(name);
  String_delete(line);

  /* close file */
  File_close(&fileHandle);

  /* free resources */

  return !failFlag;
}

/***********************************************************************\
* Name   : cmdOptionParseString
* Purpose: command line option call back for parsing string
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseString(void *userData, void *variable, const char *name, const char *value, const void *defaultValue)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

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
* Purpose: command line option call back for parsing config file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseConfigFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue)
{
  assert(value != NULL);

  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  StringList_appendCString(&configFileNameList,value);

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseIncludeExclude
* Purpose: command line option call back for parsing include/exclude
*          patterns
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseIncludeExclude(void *userData, void *variable, const char *name, const char *value, const void *defaultValue)
{
  PatternTypes patternType;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  /* detect pattern type, get pattern */
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  /* append to list */
  if (PatternList_appendCString((PatternList*)variable,value,patternType) != ERROR_NONE)
  {
    fprintf(stderr,"Cannot parse varlue '%s' of option '%s'!\n",value,name);
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParsePassword
* Purpose: command line option call back for parsing password
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  if ((*(Password**)variable) == NULL)
  {
    (*(Password**)variable) = Password_new();
  }
  Password_setCString(*(Password**)variable,value);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseConfigFile
* Purpose: command line option call back for parsing config file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value)
{
  assert(value != NULL);

  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(userData);

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
  CmdOption_printHelp(stdout,
                      COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                      level
                     );
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
  globalOptions.ftpServerList    = &ftpServerList;
  globalOptions.sshServerList    = &sshServerList;
  globalOptions.deviceList       = &deviceList;
  globalOptions.defaultFTPServer = &defaultFTPServer;
  globalOptions.defaultSSHServer = &defaultSSHServer;
  globalOptions.defaultDevice    = &defaultDevice;
  globalOptions.ftpServer        = globalOptions.defaultFTPServer;
  globalOptions.sshServer        = globalOptions.defaultSSHServer;
  globalOptions.device           = globalOptions.defaultDevice;
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
  String_delete(globalOptions.defaultDeviceName);
  
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

  String_delete(globalOptions.remoteBARExecutable);
  String_delete(globalOptions.tmpDirectory);
}

/***********************************************************************\
* Name   : freeFTPServerNode
* Purpose: free FTP server node
* Input  : ftpServerNode - FTP server node
*          userData      - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFTPServerNode(FTPServerNode *ftpServerNode, void *userData)
{
  assert(ftpServerNode != NULL);

  UNUSED_VARIABLE(userData);

  Password_delete(ftpServerNode->ftpServer.password);
  String_delete(ftpServerNode->ftpServer.loginName);
  String_delete(ftpServerNode->name);
  LIST_DELETE_NODE(ftpServerNode);
}

/***********************************************************************\
* Name   : freeSSHServerNode
* Purpose: free SSH server node
* Input  : sshServerNode - SSH server node
*          userData      - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeSSHServerNode(SSHServerNode *sshServerNode, void *userData)
{
  assert(sshServerNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(sshServerNode->sshServer.privateKeyFileName);
  String_delete(sshServerNode->sshServer.publicKeyFileName);
  Password_delete(sshServerNode->sshServer.password);
  String_delete(sshServerNode->sshServer.loginName);
  String_delete(sshServerNode->name);
  LIST_DELETE_NODE(sshServerNode);
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
  String_delete(deviceNode->name);
}

/***********************************************************************\
* Name   : initAll
* Purpose: initialize
* Input  : -
* Output : -
* Return : TRUE if init ok, FALSE on error
* Notes  : -
\***********************************************************************/

LOCAL bool initAll(void)
{
  Errors error;

  /* initialise modules */
  error = Password_initAll();
  if (error != ERROR_NONE)
  {
    return FALSE;
  }
  error = Crypt_initAll();
  if (error != ERROR_NONE)
  {
    Password_doneAll();
    return FALSE;
  }
  error = Pattern_initAll();
  if (error != ERROR_NONE)
  {
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }
  error = PatternList_initAll();
  if (error != ERROR_NONE)
  {
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }
  error = Archive_initAll();
  if (error != ERROR_NONE)
  {
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }
  error = Storage_initAll();
  if (error != ERROR_NONE)
  {
    Archive_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }
  error = Network_initAll();
  if (error != ERROR_NONE)
  {
    Storage_doneAll();
    Archive_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }
  error = Server_initAll();
  if (error != ERROR_NONE)
  {
    Network_doneAll();
    Storage_doneAll();
    Archive_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }

  /* initialise variables */
  StringList_init(&configFileNameList);
  tmpDirectory = String_new();
  tmpLogFileName = String_new();
  outputLine = String_new();
  outputNewLineFlag = TRUE;
  initGlobalOptions();
  List_init(&ftpServerList);
  List_init(&sshServerList);
  List_init(&deviceList);
  serverPassword = Password_new();
  PatternList_init(&includePatternList);
  PatternList_init(&excludePatternList);

  return TRUE;
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
  /* deinitialise variables */
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
  if (defaultSSHServer.privateKeyFileName != NULL) String_delete(defaultSSHServer.privateKeyFileName);
  if (defaultSSHServer.publicKeyFileName != NULL) String_delete(defaultSSHServer.publicKeyFileName);
  if (defaultSSHServer.loginName != NULL) Password_delete(defaultSSHServer.password);
  if (defaultSSHServer.loginName != NULL) String_delete(defaultSSHServer.loginName);
  if (defaultFTPServer.password != NULL) Password_delete(defaultFTPServer.password);
  if (defaultFTPServer.loginName != NULL) String_delete(defaultFTPServer.loginName);
  PatternList_done(&excludePatternList);
  PatternList_done(&includePatternList);
  Password_delete(serverPassword);
  freeJobOptions(&jobOptions);
  List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
  List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
  List_done(&ftpServerList,(ListNodeFreeFunction)freeFTPServerNode,NULL);
  doneGlobalOptions();
  String_delete(outputLine);
  String_delete(tmpLogFileName);
  String_delete(tmpDirectory);
  StringList_done(&configFileNameList);
  String_delete(keyFileName);

  /* deinitialise modules */
  Server_doneAll();
  Network_doneAll();
  Storage_doneAll();
  Archive_doneAll();
  PatternList_doneAll();
  Pattern_doneAll();
  Crypt_doneAll();
  Password_doneAll();
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

void vprintInfo(uint verboseLevel, const char *prefix, const char *format, va_list arguments)
{
  String line;

  assert(format != NULL);

  if (!globalOptions.quietFlag && (globalOptions.verboseLevel >= verboseLevel))
  {
    line = String_new();

    /* format line */
    if (prefix != NULL) String_appendCString(line,prefix);
    String_vformat(line,format,arguments);

    /* output */
    output(stdout,FALSE,line);

    String_delete(line);
  }
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
  String dateTime;

  assert(text != NULL);

  if ((tmpLogFile != NULL) || (logFile != NULL))
  {
    if ((logType == LOG_TYPE_ALWAYS) || ((logTypes & logType) != 0))
    {
      dateTime = Misc_formatDateTime(String_new(),Misc_getCurrentDateTime(),NULL);

      if (tmpLogFile != NULL)
      {
        /* append to temporary log file */
        fprintf(tmpLogFile,"%s> ",String_cString(dateTime));
        if (prefix != NULL) fprintf(tmpLogFile,prefix);
        vfprintf(tmpLogFile,text,arguments);
        fprintf(tmpLogFile,"\n");
      }

      if (logFile != NULL)
      {
        /* append to log file */
        fprintf(logFile,"%s> ",String_cString(dateTime));
        if (prefix != NULL) fprintf(logFile,prefix);
        vfprintf(logFile,text,arguments);
        fprintf(logFile,"\n");
      }

      String_delete(dateTime);
    }
  }
}

void logMessage(ulong logType, const char *text, ...)
{
  va_list arguments;

  assert(text != NULL);

  va_start(arguments,text);
  vlogMessage(logType,NULL,text,arguments);
  va_end(arguments);
}

void printConsole(const char *format, ...)
{
  String  line;
  va_list arguments;

  assert(format != NULL);

  line = String_new();

  /* format line */
  va_start(arguments,format);
  String_vformat(line,format,arguments);
  va_end(arguments);

  /* output */
  output(stdout,FALSE,line);

  String_delete(line);
}

void printWarning(const char *text, ...)
{
  va_list arguments;
  String  line;

  assert(text != NULL);

  /* output log line */
  va_start(arguments,text);
  vlogMessage(LOG_TYPE_WARNING,"Warning: ",text,arguments);
  va_end(arguments);

  /* format line */
  va_start(arguments,text);
  line = String_new();
  String_appendCString(line,"Warning: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  /* output */
  output(stdout,TRUE,line);

  String_delete(line);
}

void printError(const char *text, ...)
{
  va_list arguments;
  String  line;

  assert(text != NULL);

  /* output log line */
  va_start(arguments,text);
  vlogMessage(LOG_TYPE_ERROR,"ERROR: ",text,arguments);
  va_end(arguments);

  /* format line */
  va_start(arguments,text);
  line = String_new();
  String_appendCString(line,"ERROR: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  /* output console */
  output(stderr,TRUE,line);

  String_delete(line);
}

void logPostProcess(void)
{
  TextMacro textMacros[1];
  Errors    error;

  /* flush logs */
  if (logFile != NULL) fflush(logFile);
  if (tmpLogFile != NULL) fflush(tmpLogFile);

  /* log post command */
  if (logPostCommand != NULL)
  {
    printInfo(2,"Log post process...");
    TEXT_MACRO_N_STRING(textMacros[0],"%file",tmpLogFileName);
    error = Misc_executeCommand(logPostCommand,
                                textMacros,SIZE_OF_ARRAY(textMacros),
                                NULL,
                                NULL,
                                NULL
                               );
    if (error == ERROR_NONE)
    {
      printInfo(2,"ok\n");
    }
    else
    {
      printInfo(2,"FAIL\n");
      printError("Cannot post-process log file (error: %s)\n",Errors_getText(error));
    }
  }

  /* reset temporary log file */
  if (tmpLogFile != NULL) fclose(tmpLogFile);
  tmpLogFile = fopen(String_cString(tmpLogFileName),"w");
}

void initJobOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  memset(jobOptions,0,sizeof(JobOptions));
}

void copyJobOptions(const JobOptions *sourceJobOptions, JobOptions *destinationJobOptions)
{
  assert(sourceJobOptions != NULL);
  assert(destinationJobOptions != NULL);

  memcpy(destinationJobOptions,sourceJobOptions,sizeof(JobOptions));
  destinationJobOptions->incrementalListFileName      = String_duplicate(sourceJobOptions->incrementalListFileName);
  destinationJobOptions->directory                    = String_duplicate(sourceJobOptions->directory);
  destinationJobOptions->cryptPassword                = Password_duplicate(sourceJobOptions->cryptPassword);
  destinationJobOptions->cryptPublicKeyFileName       = String_duplicate(sourceJobOptions->cryptPublicKeyFileName);
  destinationJobOptions->cryptPrivateKeyFileName      = String_duplicate(sourceJobOptions->cryptPrivateKeyFileName);
  destinationJobOptions->ftpServer.loginName          = String_duplicate(sourceJobOptions->ftpServer.loginName);
  destinationJobOptions->ftpServer.password           = Password_duplicate(sourceJobOptions->ftpServer.password);
  destinationJobOptions->sshServer.loginName          = String_duplicate(sourceJobOptions->sshServer.loginName);
  destinationJobOptions->sshServer.password           = Password_duplicate(sourceJobOptions->sshServer.password);
  destinationJobOptions->sshServer.publicKeyFileName  = String_duplicate(sourceJobOptions->sshServer.publicKeyFileName);
  destinationJobOptions->sshServer.privateKeyFileName = String_duplicate(sourceJobOptions->sshServer.privateKeyFileName);
  destinationJobOptions->deviceName                   = String_duplicate(sourceJobOptions->deviceName);
}

void freeJobOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  String_delete(jobOptions->sshServer.privateKeyFileName);
  String_delete(jobOptions->sshServer.publicKeyFileName);
  Password_delete(jobOptions->sshServer.password);
  String_delete(jobOptions->sshServer.loginName);
  Password_delete(jobOptions->ftpServer.password);
  String_delete(jobOptions->ftpServer.loginName);
  String_delete(jobOptions->cryptPrivateKeyFileName);
  String_delete(jobOptions->cryptPublicKeyFileName);
  Password_delete(jobOptions->cryptPassword);
  String_delete(jobOptions->directory);
  String_delete(jobOptions->incrementalListFileName);
  memset(jobOptions,0,sizeof(JobOptions));
}

void getFTPServer(const String     name,
                  const JobOptions *jobOptions,
                  FTPServer        *ftpServer
                 )
{
  FTPServerNode *ftpServerNode;

  assert(name != NULL);
  assert(jobOptions != NULL);
  assert(ftpServer != NULL);

  ftpServerNode = globalOptions.ftpServerList->head;
  while ((ftpServerNode != NULL) && !String_equals(ftpServerNode->name,name))
  {
    ftpServerNode = ftpServerNode->next;
  }
  ftpServer->loginName = !String_empty(jobOptions->ftpServer.loginName )?jobOptions->ftpServer.loginName:((ftpServerNode != NULL)?ftpServerNode->ftpServer.loginName:globalOptions.defaultFTPServer->loginName);
  ftpServer->password  = !Password_empty(jobOptions->ftpServer.password)?jobOptions->ftpServer.password :((ftpServerNode != NULL)?ftpServerNode->ftpServer.password :globalOptions.defaultFTPServer->password );
}

void getSSHServer(const String     name,
                  const JobOptions *jobOptions,
                  SSHServer        *sshServer
                 )
{
  SSHServerNode *sshServerNode;

  assert(name != NULL);
  assert(jobOptions != NULL);
  assert(sshServer != NULL);

  sshServerNode = globalOptions.sshServerList->head;
  while ((sshServerNode != NULL) && !String_equals(sshServerNode->name,name))
  {
    sshServerNode = sshServerNode->next;
  }
  sshServer->port               = (jobOptions->sshServer.port != 0                      )?jobOptions->sshServer.port              :((sshServerNode != NULL)?sshServerNode->sshServer.port              :globalOptions.defaultSSHServer->port              );
  sshServer->loginName          = !String_empty(jobOptions->sshServer.loginName         )?jobOptions->sshServer.loginName         :((sshServerNode != NULL)?sshServerNode->sshServer.loginName         :globalOptions.defaultSSHServer->loginName         );
  sshServer->password           = !Password_empty(jobOptions->sshServer.password        )?jobOptions->sshServer.password          :((sshServerNode != NULL)?sshServerNode->sshServer.password          :globalOptions.defaultSSHServer->password          );
  sshServer->publicKeyFileName  = !String_empty(jobOptions->sshServer.publicKeyFileName )?jobOptions->sshServer.publicKeyFileName :((sshServerNode != NULL)?sshServerNode->sshServer.publicKeyFileName :globalOptions.defaultSSHServer->publicKeyFileName );
  sshServer->privateKeyFileName = !String_empty(jobOptions->sshServer.privateKeyFileName)?jobOptions->sshServer.privateKeyFileName:((sshServerNode != NULL)?sshServerNode->sshServer.privateKeyFileName:globalOptions.defaultSSHServer->privateKeyFileName);
}

void getDevice(const String     name,
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
  device->requestVolumeCommand    = (jobOptions->device.requestVolumeCommand    != NULL)?jobOptions->device.requestVolumeCommand   :((deviceNode != NULL)?deviceNode->device.requestVolumeCommand   :globalOptions.defaultDevice->requestVolumeCommand   );
  device->unloadVolumeCommand     = (jobOptions->device.unloadVolumeCommand     != NULL)?jobOptions->device.unloadVolumeCommand    :((deviceNode != NULL)?deviceNode->device.unloadVolumeCommand    :globalOptions.defaultDevice->unloadVolumeCommand    );
  device->loadVolumeCommand       = (jobOptions->device.loadVolumeCommand       != NULL)?jobOptions->device.loadVolumeCommand      :((deviceNode != NULL)?deviceNode->device.loadVolumeCommand      :globalOptions.defaultDevice->loadVolumeCommand      );
  device->volumeSize              = (jobOptions->device.volumeSize              != 0   )?jobOptions->device.volumeSize             :((deviceNode != NULL)?deviceNode->device.volumeSize             :globalOptions.defaultDevice->volumeSize             );
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

Errors inputCryptPassword(Password     *password,
                          const String fileName,
                          bool         validateFlag,
                          bool         weakCheckFlag
                         )
{
  Errors error;

  assert(password != NULL);
  assert(fileName != NULL);

  switch (globalOptions.runMode)
  {
    case RUN_MODE_INTERACTIVE:
      {
        String title;
        /* initialise variables */
        title = String_new();

        /* input password */
        String_format(String_clear(title),"Crypt password for '%S'",fileName);
        if (!Password_input(password,String_cString(title),PASSWORD_INPUT_MODE_ANY) || (Password_length(password) <= 0))
        {
          String_delete(title);
          error = ERROR_NO_CRYPT_PASSWORD;
          break;
        }
        if (validateFlag)
        {
          String_format(String_clear(title),"Verify password for '%S'",fileName);
          if (!Password_inputVerify(password,String_cString(title),PASSWORD_INPUT_MODE_ANY))
          {
            printError("Passwords are not equal!\n");
            String_delete(title);
            error = ERROR_PASSWORDS_MISMATCH;
            break;
          }
        }
        String_delete(title);

        if (weakCheckFlag)
        {
          /* check password quality */
          if (Password_getQualityLevel(password) < MIN_PASSWORD_QUALITY_LEVEL)
          {
            printWarning("Low password quality!\n");
          }
        }

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

  return error;
}

bool configValueParseIncludeExclude(void *userData, void *variable, const char *name, const char *value)
{
  PatternTypes patternType;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  /* detect pattern type, get pattern */
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  /* append to list */
  if (PatternList_appendCString((PatternList*)variable,value,patternType) != ERROR_NONE)
  {
    fprintf(stderr,"Cannot parse varlue '%s' of option '%s'!\n",value,name);
    return FALSE;
  }

  return TRUE;
}

void configValueFormatInitIncludeExclude(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((PatternList*)variable)->head;
}

void configValueFormatDoneIncludeExclude(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(formatUserData);  
}

bool configValueFormatIncludeExclude(void **formatUserData, void *userData, String line)
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

bool configValueParsePassword(void *userData, void *variable, const char *name, const char *value)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

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

/***********************************************************************\
* Name   : parseScheduleNumber
* Purpose: parse schedule number (year, day, month, hour, minute)
* Input  : s - string to parse
* Output : n - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseScheduleNumber(const String s, int *n)
{
  long nextIndex;

  assert(s != NULL);
  assert(n != NULL);

  /* init variables */
  if   (String_equalsCString(s,"*"))
  {
    (*n) = SCHEDULE_ANY;
  }
  else 
  {
    (*n) = (uint)String_toInteger(s,0,&nextIndex,NULL,0);
    if (nextIndex != STRING_END) return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : parseScheduleMonth
* Purpose: parse month name
* Input  : s - string to parse
* Output : month - month (MONTH_JAN..MONTH_DEC)
* Return : TRUE iff month parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseScheduleMonth(const String s, int *month)
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
  else if (String_equalsCString(name,"jan")) (*month) = MONTH_JAN;
  else if (String_equalsCString(name,"feb")) (*month) = MONTH_FEB;
  else if (String_equalsCString(name,"mar")) (*month) = MONTH_MAR;
  else if (String_equalsCString(name,"arp")) (*month) = MONTH_APR;
  else if (String_equalsCString(name,"may")) (*month) = MONTH_MAY;
  else if (String_equalsCString(name,"jun")) (*month) = MONTH_JUN;
  else if (String_equalsCString(name,"jul")) (*month) = MONTH_JUL;
  else if (String_equalsCString(name,"aug")) (*month) = MONTH_AUG;
  else if (String_equalsCString(name,"sep")) (*month) = MONTH_SEP;
  else if (String_equalsCString(name,"oct")) (*month) = MONTH_OCT;
  else if (String_equalsCString(name,"nov")) (*month) = MONTH_NOV;
  else if (String_equalsCString(name,"dec")) (*month) = MONTH_DEC;
  else
  {
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
* Name   : parseScheduleWeekDay
* Purpose: parse week day
* Input  : s - string to parse
* Output : weekday - week day
* Return : TRUE iff week day parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseScheduleWeekDay(const String s, int *weekday)
{
  String name;

  assert(s != NULL);
  assert(weekday != NULL);

  name = String_toLower(String_duplicate(s));
  if (String_equalsCString(s,"*"))
  {
    (*weekday) = SCHEDULE_ANY;
  }
  else if (String_equalsCString(name,"mon")) (*weekday) = WEEKDAY_MON;
  else if (String_equalsCString(name,"tue")) (*weekday) = WEEKDAY_TUE;
  else if (String_equalsCString(name,"wed")) (*weekday) = WEEKDAY_WED;
  else if (String_equalsCString(name,"thu")) (*weekday) = WEEKDAY_THU;
  else if (String_equalsCString(name,"fri")) (*weekday) = WEEKDAY_FRI;
  else if (String_equalsCString(name,"sat")) (*weekday) = WEEKDAY_SAT;
  else if (String_equalsCString(name,"sun")) (*weekday) = WEEKDAY_SUN;
  else
  {
    String_delete(name);
    return FALSE;
  }
  String_delete(name);

  return TRUE;
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
  else if (String_equalsCString(name,"normal"     )) (*archiveType) = ARCHIVE_TYPE_NORMAL;
  else if (String_equalsCString(name,"full"       )) (*archiveType) = ARCHIVE_TYPE_FULL;
  else if (String_equalsCString(name,"incremental")) (*archiveType) = ARCHIVE_TYPE_INCREMENTAL;
  else
  {
    String_delete(name);
    return FALSE;
  }
  String_delete(name);

  return TRUE;
}

ScheduleNode *parseSchedule(const String s)
{
  ScheduleNode *scheduleNode;
  bool         errorFlag;
  String       s0,s1,s2;
  bool         b;
  ulong        nextIndex;

  assert(s != NULL);

  /* allocate new schedule node */
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
  scheduleNode->weekDay     = SCHEDULE_ANY;
  scheduleNode->archiveType = ARCHIVE_TYPE_NORMAL;
  scheduleNode->enabled     = FALSE;

  /* parse schedule: date, weekday, time, type, enabled */
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  nextIndex = STRING_BEGIN;
  if      (String_parse(s,nextIndex,"%S-%S-%S",&nextIndex,s0,s1,s2))
  {
    parseScheduleNumber(s0,&scheduleNode->year);
    if (!parseScheduleMonth (s1,&scheduleNode->month)) errorFlag = TRUE;
    parseScheduleNumber(s2,&scheduleNode->day);
  }
  else
  {
    errorFlag = TRUE;
  }
  if      (String_parse(s,nextIndex,"%S %S:%S",&nextIndex,s0,s1,s2))
  {
    if (!parseScheduleWeekDay(s0,&scheduleNode->weekDay)) errorFlag = TRUE;
    parseScheduleNumber(s1,&scheduleNode->hour  );
    parseScheduleNumber(s2,&scheduleNode->minute);
  }
  else if (String_parse(s,nextIndex,"%S:%S",&nextIndex,s0,s1))
  {
    parseScheduleNumber(s0,&scheduleNode->hour  );
    parseScheduleNumber(s1,&scheduleNode->minute);
  }
  else
  {
    errorFlag = TRUE;
  }
  if (String_parse(s,nextIndex,"%y",&nextIndex,&b))
  {
    scheduleNode->enabled = b;
  }
  else
  {
    errorFlag = TRUE;
  }
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

bool configValueParseSchedule(void *userData, void *variable, const char *name, const char *value)
{
  ScheduleNode *scheduleNode;
  String       s;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  /* allocate new schedule node */
  s = String_newCString(value);
  scheduleNode = parseSchedule(s);
  if (scheduleNode == NULL)
  {
    String_delete(s);
    return FALSE;
  }
  String_delete(s);

  /* append to list */
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

    if (scheduleNode->weekDay != SCHEDULE_ANY)
    {
      switch (scheduleNode->weekDay)
      {
        case WEEKDAY_MON: String_appendCString(line,"Mon"); break;
        case WEEKDAY_TUE: String_appendCString(line,"Tue"); break;
        case WEEKDAY_WED: String_appendCString(line,"Wed"); break;
        case WEEKDAY_THU: String_appendCString(line,"Thu"); break;
        case WEEKDAY_FRI: String_appendCString(line,"Fri"); break;
        case WEEKDAY_SAT: String_appendCString(line,"Sat"); break;
        case WEEKDAY_SUN: String_appendCString(line,"Sun"); break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      String_appendChar(line,' ');
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
    fileName = File_newFileName();
    error = File_open(&fileHandle,File_setFileNameCString(fileName,pidFileName),FILE_OPENMODE_CREATE);
    if (error != ERROR_NONE)
    {
      printError("Cannot create process id file '%s' (error: %s)\n",pidFileName,Errors_getText(error));
      return error;
    }
    File_printLine(&fileHandle,"%d",(int)getpid());
    File_close(&fileHandle);
    File_deleteFileName(fileName);
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
  String fileName;
  bool   printInfoFlag;
  Errors error;

  /* init */
  if (!initAll())
  {
    #ifndef NDEBUG
      Array_debug();
      String_debug();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_INIT_FAIL;
  }
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  globalOptions.barExecutable = argv[0];

  /* parse command line: pre-options */
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,
                       stderr,NULL
                      )
     )
  {
    doneAll();
    #ifndef NDEBUG
      Array_debug();
      String_debug();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_INVALID_ARGUMENT;
  }

  if (!globalOptions.noDefaultConfigFlag)
  {
    fileName = String_new();

    /* read default configuration from /CONFIG_DIR/bar.cfg (ignore errors) */
    File_setFileNameCString(fileName,CONFIG_DIR);
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName) && File_isReadable(fileName))
    {
      StringList_append(&configFileNameList,fileName);
    }

    /* read default configuration from $HOME/.bar/bar.cfg (if exists) */
    File_setFileNameCString(fileName,getenv("HOME"));
    File_appendFileNameCString(fileName,".bar");
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName))
    {
      StringList_append(&configFileNameList,fileName);
    }

    String_delete(fileName);
  }

  /* read all configuration files */
  fileName = String_new();
  printInfoFlag = daemonFlag;
  while (StringList_getFirst(&configFileNameList,fileName) != NULL)
  {
    if (!readConfigFile(fileName,printInfoFlag))
    {
      String_delete(fileName);
      doneAll();
      #ifndef NDEBUG
        Array_debug();
        String_debug();
        String_debugDone();
      #endif /* not NDEBUG */
      return EXITCODE_CONFIG_ERROR;
    }
  }
  String_delete(fileName);

  /* parse command line: all */
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       CMD_PRIORITY_ANY,
                       stderr,NULL
                      )
     )
  {
    doneAll();
    #ifndef NDEBUG
      Array_debug();
      String_debug();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_INVALID_ARGUMENT;
  }

  /* output version, help */
  if (versionFlag)
  {
    #ifndef NDEBUG
      printf("BAR version %s (debug)\n",VERSION_STRING);
    #else /* NDEBUG */
      printf("BAR version %s\n",VERSION_STRING);
    #endif /* not NDEBUG */

    doneAll();
    #ifndef NDEBUG
      Array_debug();
      String_debug();
      String_debugDone();
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
      Array_debug();
      String_debug();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_OK;
  }

  /* check parameters */
  if (!validateOptions())
  {
    doneAll();
    #ifndef NDEBUG
      Array_debug();
      String_debug();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_FAIL;
  }

  /* create temporary directory */
  error = File_getTmpDirectoryName(tmpDirectory,NULL,globalOptions.tmpDirectory);
  if (error != ERROR_NONE)
  {
    printError("Cannot create temporary directory in '%s' (error: %s)!\n",String_cString(globalOptions.tmpDirectory),Errors_getText(error));
    doneAll();
    #ifndef NDEBUG
      Array_debug();
      String_debug();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_FAIL;
  }

  /* create session log file */
  File_setFileName(tmpLogFileName,tmpDirectory);
  File_appendFileNameCString(tmpLogFileName,"log.txt");
  tmpLogFile = fopen(String_cString(tmpLogFileName),"w");

  /* open log files */
  if (logFileName != NULL)
  {
    logFile = fopen(logFileName,"a");
    if (logFile == NULL) printWarning("Cannot open log file '%s' (error: %s)!\n",logFileName,strerror(errno));
  }

  error = ERROR_NONE;
  if      (daemonFlag)
  {    
    /* daemon mode -> run server with network */
    globalOptions.runMode = RUN_MODE_SERVER;

    if (!noDetachFlag)
    {
      /* run server (detached) */
      if (daemon(1,0) == 0)
      {
        if (pidFileName != NULL)
        {
          /* create pid file */
          error = createPIDFile();
        }

        if (error == ERROR_NONE)
        {
          /* run server */
          error = Server_run(serverPort,
                             serverTLSPort,
                             serverCAFileName,
                             serverCertFileName,
                             serverKeyFileName,
                             serverPassword,
                             serverJobsDirectory,
                             &jobOptions
                            );
        }

        if (pidFileName != NULL)
        {
          /* delete pid file */
          deletePIDFile();
        }

        /* close log files */
        if (logFile != NULL) fclose(logFile);
        fclose(tmpLogFile);unlink(String_cString(tmpLogFileName));
        File_delete(tmpLogFileName,FALSE);

        /* free resources */
        CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
        doneAll();
        #ifndef NDEBUG
          Array_debug();
          String_debug();
          String_debugDone();
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
    }
    else
    {
      if (pidFileName != NULL)
      {
        /* create pid file */
        error = createPIDFile();
        if (error != ERROR_NONE)
        {
          printError("Cannot create process id file '%s' (error: %s)\n",pidFileName,Errors_getText(error));
        }
      }

      if (error == ERROR_NONE)
      {
        /* run server (not detached) */
        error = Server_run(serverPort,
                           serverTLSPort,
                           serverCAFileName,
                           serverCertFileName,
                           serverKeyFileName,
                           serverPassword,
                           serverJobsDirectory,
                           &jobOptions
                          );
      }

      if (pidFileName != NULL)
      {
        /* delete pid file */
        deletePIDFile();
      }
    }
  }
  else if (batchFlag)
  {
    globalOptions.runMode = RUN_MODE_BATCH;

    /* batch mode -> run server with standard i/o */
    error = Server_batch(STDIN_FILENO,
                         STDOUT_FILENO
                        );
  }
  else
  {
    globalOptions.runMode = RUN_MODE_INTERACTIVE;

    switch (command)
    {
      case COMMAND_CREATE:
        {
          int z;

          /* get archive file name */
          if (argc <= 1)
          {
            printError("No archive file name given!\n");
            error = ERROR_INVALID_ARGUMENT;
            break;
          }
          archiveFileName = argv[1];

          /* get include patterns */
          for (z = 2; z < argc; z++)
          {
            error = PatternList_appendCString(&includePatternList,argv[z],jobOptions.patternType);
          }

          /* create archive */
          error = Command_create(archiveFileName,
                                 &includePatternList,
                                 &excludePatternList,
                                 &jobOptions,
                                 ARCHIVE_TYPE_NORMAL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL
                                );
        }
        break;
      case COMMAND_LIST:
      case COMMAND_TEST:
      case COMMAND_COMPARE:
      case COMMAND_RESTORE:
        {
          StringList fileNameList;
          int        z;

          /* get archive files */
          StringList_init(&fileNameList);
          for (z = 1; z < argc; z++)
          {
            StringList_appendCString(&fileNameList,argv[z]);
          }

          switch (command)
          {
            case COMMAND_LIST:
              error = Command_list(&fileNameList,
                                   &includePatternList,
                                   &excludePatternList,
                                   &jobOptions
                                  );
              break;
            case COMMAND_TEST:
              error = Command_test(&fileNameList,
                                   &includePatternList,
                                   &excludePatternList,
                                   &jobOptions
                                  );
              break;
            case COMMAND_COMPARE:
              error = Command_compare(&fileNameList,
                                      &includePatternList,
                                      &excludePatternList,
                                      &jobOptions
                                     );
              break;
            case COMMAND_RESTORE:
              error = Command_restore(&fileNameList,
                                      &includePatternList,
                                      &excludePatternList,
                                      &jobOptions,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL
                                     );
              break;
            default:
              break;
          }

          /* free resources */
          StringList_done(&fileNameList);

          /* log post command */
          logPostProcess();
        }
        break;
      case COMMAND_GENERATE_KEYS:
        {
          /* generate new key pair */
          const char *keyFileName;
          Password   cryptPassword;
          CryptKey   publicKey,privateKey;
          String     publicKeyFileName,privateKeyFileName;

          /* get key file name */
          if (argc <= 1)
          {
            printError("No key file name given!\n");
            printUsage(argv[0],0);
            error = ERROR_INVALID_ARGUMENT;
            break;
          }
          keyFileName = argv[1];

          /* initialise variables */
          publicKeyFileName  = File_newFileName();
          privateKeyFileName = File_newFileName();

          /* get file names of keys */
          File_setFileNameCString(publicKeyFileName,keyFileName);
          String_appendCString(publicKeyFileName,".public");
          File_setFileNameCString(privateKeyFileName,keyFileName);
          String_appendCString(privateKeyFileName,".private");

          /* check if key files already exists */
          if (File_exists(publicKeyFileName))
          {
            printError("Public key file '%s' already exists!\n",String_cString(publicKeyFileName));
            File_deleteFileName(privateKeyFileName);
            File_deleteFileName(publicKeyFileName);
            break;
          }
          if (File_exists(privateKeyFileName))
          {
            printError("Private key file '%s' already exists!\n",String_cString(privateKeyFileName));
            File_deleteFileName(privateKeyFileName);
            File_deleteFileName(publicKeyFileName);
            break;
          }

          /* input crypt password for private key encryption */
          Password_init(&cryptPassword);
          error = inputCryptPassword(&cryptPassword,privateKeyFileName,TRUE,FALSE);
          if (error != ERROR_NONE)
          {
            printError("No password given for private key!\n");
            Password_done(&cryptPassword);
            File_deleteFileName(privateKeyFileName);
            File_deleteFileName(publicKeyFileName);
            break;
          }

          /* generate new keys pair */
          Crypt_initKey(&publicKey);
          Crypt_initKey(&privateKey);
          error = Crypt_createKeys(&publicKey,&privateKey,keyBits);
          if (error == ERROR_NONE)
          {
            error = Crypt_writeKeyFile(&publicKey,publicKeyFileName,NULL);
            if (error != ERROR_NONE)
            {
              printError("Cannot write public key file!\n");
              Crypt_doneKey(&privateKey);
              Crypt_doneKey(&publicKey);
              Password_done(&cryptPassword);
              File_deleteFileName(privateKeyFileName);
              File_deleteFileName(publicKeyFileName);
              break;
            }

            error = Crypt_writeKeyFile(&privateKey,privateKeyFileName,&cryptPassword);
            if (error != ERROR_NONE)
            {
              printError("Cannot write private key file!\n");
              Crypt_doneKey(&privateKey);
              Crypt_doneKey(&publicKey);
              Password_done(&cryptPassword);
              File_deleteFileName(privateKeyFileName);
              File_deleteFileName(publicKeyFileName);
              break;
            }
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
          /* free resources */
          Crypt_doneKey(&privateKey);
          Crypt_doneKey(&publicKey);
          Password_done(&cryptPassword);
          File_deleteFileName(privateKeyFileName);
          File_deleteFileName(publicKeyFileName);
        }
        break;
      default:
        printError("No command given!\n");
        error = ERROR_INVALID_ARGUMENT;
        break;
    }
  }

  /* close log files */
  if (logFile != NULL) fclose(logFile);
  fclose(tmpLogFile);
  unlink(String_cString(tmpLogFileName));

  /* free resources */
  File_delete(tmpDirectory,TRUE);
  CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  doneAll();

  #ifndef NDEBUG
    Array_debug();
    String_debug();
    String_debugDone();
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
