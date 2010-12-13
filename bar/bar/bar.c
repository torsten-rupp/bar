/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/bar.c,v $
* $Revision: 1.37 $
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
#include "entrylists.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "archive.h"
#include "network.h"
#include "storage.h"
#include "database.h"
#include "index.h"
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

#define DEFAULT_CONFIG_FILE_NAME              "bar.cfg"
#define DEFAULT_TMP_DIRECTORY                 "/tmp"
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
#define DEFAULT_DEVICE_NAME                   "/dev/dvd"

#define DEFAULT_DATABASE_INDEX_FILE           "/var/lib/bar/index.db"

#define CD_UNLOAD_VOLUME_COMMAND              "eject -r %device"
#define CD_LOAD_VOLUME_COMMAND                "eject -t %device"
#define CD_IMAGE_COMMAND                      "nice mkisofs -V Backup -volset %number -r -o %image %file"
#define CD_ECC_COMMAND                        "nice dvdisaster -mRS02 -n dvd -c -i %image -v"
#define CD_WRITE_COMMAND                      "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %file"
#define CD_WRITE_IMAGE_COMMAND                "nice growisofs -Z %device=%image -use-the-force-luke=dao:%sectors -use-the-force-luke=noload"

#define DVD_UNLOAD_VOLUME_COMMAND             "eject -r %device"
#define DVD_LOAD_VOLUME_COMMAND               "eject -t %device"
#define DVD_IMAGE_COMMAND                     "nice mkisofs -V Backup -volset %number -r -o %image %file"
#define DVD_ECC_COMMAND                       "nice dvdisaster -mRS02 -n dvd -c -i %image -v"
#define DVD_WRITE_COMMAND                     "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %file"
#define DVD_WRITE_IMAGE_COMMAND               "nice growisofs -Z %device=%image -use-the-force-luke=dao:%sectors -use-the-force-luke=noload"

#define BD_UNLOAD_VOLUME_COMMAND              "eject -r %device"
#define BD_LOAD_VOLUME_COMMAND                "eject -t %device"
#define BD_IMAGE_COMMAND                      "nice mkisofs -V Backup -volset %number -r -o %image %file"
#define BD_ECC_COMMAND                        "nice dvdisaster -mRS02 -n dvd -c -i %image -v"
#define BD_WRITE_COMMAND                      "nice growisofs -Z %device -A BAR -V Backup -volset %number -r %file"
#define BD_WRITE_IMAGE_COMMAND                "nice growisofs -Z %device=%image -use-the-force-luke=dao:%sectors -use-the-force-luke=noload"

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
GlobalOptions       globalOptions;
DatabaseHandle      *indexDatabaseHandle;

LOCAL Commands      command;
LOCAL String        jobName;

LOCAL JobOptions    jobOptions;
LOCAL String        archiveName;
LOCAL FTPServerList ftpServerList;
LOCAL SSHServerList sshServerList;
LOCAL DeviceList    deviceList;
LOCAL EntryList     includeEntryList;
LOCAL PatternList   excludePatternList;
LOCAL PatternList   compressExcludePatternList;
LOCAL FTPServer     defaultFTPServer;
LOCAL SSHServer     defaultSSHServer;
LOCAL Device        defaultDevice;
LOCAL FTPServer     *currentFTPServer;
LOCAL SSHServer     *currentSSHServer;
LOCAL Device        *currentDevice;
LOCAL bool          daemonFlag;
LOCAL bool          noDetachFlag;
LOCAL uint          serverPort;
LOCAL bool          serverTLSPort;
LOCAL const char    *serverCAFileName;
LOCAL const char    *serverCertFileName;
LOCAL const char    *serverKeyFileName;
LOCAL Password      *serverPassword;
LOCAL const char    *serverJobsDirectory;

LOCAL const char    *indexDatabaseFileName;

LOCAL ulong         logTypes;
LOCAL const char    *logFileName;
LOCAL const char    *logPostCommand;

LOCAL bool          batchFlag;
LOCAL bool          versionFlag;
LOCAL bool          helpFlag,xhelpFlag,helpInternalFlag;

LOCAL const char    *pidFileName;

LOCAL String        keyFileName;
LOCAL uint          keyBits;

/*---------------------------------------------------------------------*/

LOCAL StringList    configFileNameList;

String              tmpDirectory;
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

LOCAL bool cmdOptionParseOwner(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParseString(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParseConfigFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParseEntry(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParseIncludeExclude(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);

LOCAL const CommandLineUnit COMMAND_LINE_BYTES_UNITS[] =
{
  {"G",1024LL*1024LL*1024LL},
  {"M",1024LL*1024LL},
  {"K",1024LL},
};

LOCAL const CommandLineUnit COMMAND_LINE_BITS_UNITS1[] =
{
  {"K",1024LL},
};

LOCAL const CommandLineUnit COMMAND_LINE_BITS_UNITS2[] =
{
  {"M",1024LL*1024LL},
  {"K",1024LL},
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

  {"all",       LOG_TYPE_ALL,                "log everything"           },
};

LOCAL CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM         ("create",                       'c',0,0,command,                                   COMMAND_CREATE_FILES,                                  "create new files archive"                                                 ),
  CMD_OPTION_ENUM         ("image",                        'm',0,0,command,                                   COMMAND_CREATE_IMAGES,                                 "create new images archive"                                                ),
  CMD_OPTION_ENUM         ("list",                         'l',0,0,command,                                   COMMAND_LIST,                                          "list contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("test",                         't',0,0,command,                                   COMMAND_TEST,                                          "test contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("compare",                      'd',0,0,command,                                   COMMAND_COMPARE,                                       "compare contents of archive with files and images"                        ),
  CMD_OPTION_ENUM         ("extract",                      'x',0,0,command,                                   COMMAND_RESTORE,                                       "restore archive"                                                          ),
  CMD_OPTION_ENUM         ("generate-keys",                0,  0,0,command,                                   COMMAND_GENERATE_KEYS,                                 "generate new public/private key pair"                                     ),
//  CMD_OPTION_ENUM         ("new-key-password",             0,  0,0,command,                                   COMMAND_NEW_KEY_PASSWORD,                            "set new private key password"                                             ),
  CMD_OPTION_INTEGER      ("generate-keys-bits",           0,  1,0,keyBits,                                   MIN_ASYMMETRIC_CRYPT_KEY_BITS,
                                                                                                              MAX_ASYMMETRIC_CRYPT_KEY_BITS,COMMAND_LINE_BITS_UNITS1,"key bits"                                                                 ),
  CMD_OPTION_STRING       ("job",                          0,  0,0,jobName,                                                                                          "execute job","name"                                                       ),

  CMD_OPTION_ENUM         ("normal",                       0,  1,1,jobOptions.archiveType,                    ARCHIVE_TYPE_NORMAL,                                   "create normal archive (no incremental list file)"                         ),
  CMD_OPTION_ENUM         ("full",                         'f',0,1,jobOptions.archiveType,                    ARCHIVE_TYPE_FULL,                                     "create full archive and incremental list file"                            ),
  CMD_OPTION_ENUM         ("incremental",                  'i',0,1,jobOptions.archiveType,                    ARCHIVE_TYPE_INCREMENTAL,                              "create incremental archive"                                               ),
  CMD_OPTION_SPECIAL      ("incremental-list-file",        'I',1,1,&jobOptions.incrementalListFileName,       cmdOptionParseString,NULL,                             "incremental list file name (default: <archive name>.bid)","file name"     ),
  CMD_OPTION_ENUM         ("differential",                 0,  1,1,jobOptions.archiveType,                    ARCHIVE_TYPE_DIFFERENTIAL,                             "create differential archive"                                              ),

  CMD_OPTION_SELECT       ("pattern-type",                 0,  1,1,jobOptions.patternType,                    COMMAND_LINE_OPTIONS_PATTERN_TYPES,                    "select pattern type"                                                      ),

  CMD_OPTION_SPECIAL      ("include",                      '#',0,2,&includeEntryList,                         cmdOptionParseEntry,NULL,                              "include pattern","pattern"                                                ),
  CMD_OPTION_SPECIAL      ("exclude",                      '!',0,2,&excludePatternList,                       cmdOptionParseIncludeExclude,NULL,                     "exclude pattern","pattern"                                                ),

  CMD_OPTION_SPECIAL      ("config",                       0,  1,0,NULL,                                      cmdOptionParseConfigFile,NULL,                         "configuration file","file name"                                           ),

  CMD_OPTION_STRING       ("tmp-directory",                0,  1,0,globalOptions.tmpDirectory,                                                                       "temporary directory","path"                                               ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",                 0,  1,0,globalOptions.maxTmpSize,                  0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,              "max. size of temporary files"                                             ),

  CMD_OPTION_INTEGER64    ("archive-part-size",            's',0,1,jobOptions.archivePartSize,                0,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,              "approximated archive part size"                                           ),

  CMD_OPTION_INTEGER      ("directory-strip",              'p',1,1,jobOptions.directoryStripCount,            0,MAX_INT,NULL,                                         "number of directories to strip on extract"                               ),
  CMD_OPTION_STRING       ("destination",                  0,  0,1,jobOptions.destination,                                                                          "destination to restore files/images","path"                                ),
  CMD_OPTION_SPECIAL      ("owner",                        0,  0,1,&jobOptions.owner,                         cmdOptionParseOwner,NULL,                              "user and group of restored files","user:group"                            ),

  CMD_OPTION_SELECT       ("compress-algorithm",           'z',0,1,jobOptions.compressAlgorithm,              COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHMS,              "select compress algorithm to use"                                         ),
  CMD_OPTION_INTEGER      ("compress-min-size",            0,  1,1,globalOptions.compressMinFileSize,         0,MAX_INT,COMMAND_LINE_BYTES_UNITS,                    "minimal size of file for compression"                                     ),
  CMD_OPTION_SPECIAL      ("compress-exclude",             0,  0,2,&compressExcludePatternList,               cmdOptionParseIncludeExclude,NULL,                     "exclude compression pattern","pattern"                                    ),

  CMD_OPTION_SELECT       ("crypt-algorithm",              'y',0,1,jobOptions.cryptAlgorithm,                 COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS,                 "select crypt algorithm to use"                                            ),
  CMD_OPTION_SELECT       ("crypt-type",                   0,  0,1,jobOptions.cryptType,                      COMMAND_LINE_OPTIONS_CRYPT_TYPES,                      "select crypt type"                                                        ),
  CMD_OPTION_SPECIAL      ("crypt-password",               0,  0,1,&globalOptions.cryptPassword,              cmdOptionParsePassword,NULL,                           "crypt password (use with care!)","password"                               ),
  CMD_OPTION_STRING       ("crypt-public-key",             0,  0,1,jobOptions.cryptPublicKeyFileName,                                                                "public key for encryption","file name"                                    ),
  CMD_OPTION_STRING       ("crypt-private-key",            0,  0,1,jobOptions.cryptPrivateKeyFileName,                                                               "private key for decryption","file name"                                   ),

  CMD_OPTION_STRING       ("ftp-login-name",               0,  0,1,defaultFTPServer.loginName,                                                                       "ftp login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ftp-password",                 0,  0,1,&defaultFTPServer.password,                cmdOptionParsePassword,NULL,                           "ftp password (use with care!)","password"                                 ),

  CMD_OPTION_INTEGER      ("ssh-port",                     0,  0,1,defaultSSHServer.port,                     0,65535,NULL,                                          "ssh port"                                                                 ),
  CMD_OPTION_STRING       ("ssh-login-name",               0,  0,1,defaultSSHServer.loginName,                                                                       "ssh login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ssh-password",                 0,  0,1,&defaultSSHServer.password,                cmdOptionParsePassword,NULL,                           "ssh password (use with care!)","password"                                 ),
  CMD_OPTION_STRING       ("ssh-public-key",               0,  1,1,defaultSSHServer.publicKeyFileName,                                                               "ssh public key file name","file name"                                     ),
  CMD_OPTION_STRING       ("ssh-private-key",              0,  1,1,defaultSSHServer.privateKeyFileName,                                                              "ssh private key file name","file name"                                    ),

  CMD_OPTION_BOOLEAN      ("daemon",                       0,  1,0,daemonFlag,                                                                                       "run in daemon mode"                                                       ),
  CMD_OPTION_BOOLEAN      ("no-detach",                    'D',1,0,noDetachFlag,                                                                                     "do not detach in daemon mode"                                             ),
  CMD_OPTION_INTEGER      ("server-port",                  0,  1,0,serverPort,                                0,65535,NULL,                                          "server port"                                                              ),
  CMD_OPTION_INTEGER      ("server-tls-port",              0,  1,0,serverTLSPort,                             0,65535,NULL,                                          "TLS (SSL) server port"                                                    ),
  CMD_OPTION_CSTRING      ("server-ca-file",               0,  1,0,serverCAFileName,                                                                                 "TLS (SSL) server certificate authority file (CA file)","file name"        ),
  CMD_OPTION_CSTRING      ("server-cert-file",             0,  1,0,serverCertFileName,                                                                               "TLS (SSL) server certificate file","file name"                            ),
  CMD_OPTION_CSTRING      ("server-key-file",              0,  1,0,serverKeyFileName,                                                                                "TLS (SSL) server key file","file name"                                    ),
  CMD_OPTION_SPECIAL      ("server-password",              0,  1,0,&serverPassword,                           cmdOptionParsePassword,NULL,                           "server password (use with care!)","password"                              ),
  CMD_OPTION_CSTRING      ("server-jobs-directory",        0,  1,0,serverJobsDirectory,                                                                              "server job directory","path name"                                        ),

  CMD_OPTION_INTEGER      ("nice-level",                   0,  1,0,globalOptions.niceLevel,                   0,19,NULL,                                             "general nice level of processes/threads"                                  ),

  CMD_OPTION_INTEGER      ("max-band-width",               0,  1,0,globalOptions.maxBandWidth,                0,MAX_INT,COMMAND_LINE_BITS_UNITS2,                    "max. network band width to use [bits/s]"                                  ),

  CMD_OPTION_BOOLEAN      ("batch",                        0,  2,0,batchFlag,                                                                                        "run in batch mode"                                                        ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",        0,  1,0,&globalOptions.remoteBARExecutable,        cmdOptionParseString,NULL,                             "remote BAR executable","file name"                                        ),

  CMD_OPTION_STRING       ("cd-request-volume-command",    0,  1,0,globalOptions.cd.requestVolumeCommand,                                                            "request new CD volume command","command"                                  ),
  CMD_OPTION_STRING       ("cd-unload-volume-command",     0,  1,0,globalOptions.cd.unloadVolumeCommand,                                                             "unload CD volume command","command"                                       ),
  CMD_OPTION_STRING       ("cd-load-volume-command",       0,  1,0,globalOptions.cd.loadVolumeCommand,                                                               "load CD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("cd-volume-size",               0,  1,0,globalOptions.cd.volumeSize,              0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,             "CD volume size"                                                           ),
  CMD_OPTION_STRING       ("cd-image-pre-command",         0,  1,0,globalOptions.cd.imagePreProcessCommand,                                                          "make CD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("cd-image-post-command",        0,  1,0,globalOptions.cd.imagePostProcessCommand,                                                         "make CD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("cd-image-command",             0,  1,0,globalOptions.cd.imageCommand,                                                                    "make CD image command","command"                                          ),
  CMD_OPTION_STRING       ("cd-ecc-pre-command",           0,  1,0,globalOptions.cd.eccPreProcessCommand,                                                            "make CD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("cd-ecc-post-command",          0,  1,0,globalOptions.cd.eccPostProcessCommand,                                                           "make CD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("cd-ecc-command",               0,  1,0,globalOptions.cd.eccCommand,                                                                      "make CD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("cd-write-pre-command",         0,  1,0,globalOptions.cd.writePreProcessCommand,                                                          "write CD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("cd-write-post-command",        0,  1,0,globalOptions.cd.writePostProcessCommand,                                                         "write CD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("cd-write-command",             0,  1,0,globalOptions.cd.writeCommand,                                                                    "write CD command","command"                                               ),
  CMD_OPTION_STRING       ("cd-write-image-command",       0,  1,0,globalOptions.cd.writeImageCommand,                                                               "write CD image command","command"                                         ),

  CMD_OPTION_STRING       ("dvd-request-volume-command",   0,  1,0,globalOptions.dvd.requestVolumeCommand,                                                           "request new DVD volume command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-unload-volume-command",    0,  1,0,globalOptions.dvd.unloadVolumeCommand,                                                            "unload DVD volume command","command"                                      ),
  CMD_OPTION_STRING       ("dvd-load-volume-command",      0,  1,0,globalOptions.dvd.loadVolumeCommand,                                                              "load DVD volume command","command"                                        ),
  CMD_OPTION_INTEGER64    ("dvd-volume-size",              0,  1,0,globalOptions.dvd.volumeSize,              0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,            "DVD volume size"                                                          ),
  CMD_OPTION_STRING       ("dvd-image-pre-command",        0,  1,0,globalOptions.dvd.imagePreProcessCommand,                                                         "make DVD image pre-process command","command"                             ),
  CMD_OPTION_STRING       ("dvd-image-post-command",       0,  1,0,globalOptions.dvd.imagePostProcessCommand,                                                        "make DVD image post-process command","command"                            ),
  CMD_OPTION_STRING       ("dvd-image-command",            0,  1,0,globalOptions.dvd.imageCommand,                                                                   "make DVD image command","command"                                         ),
  CMD_OPTION_STRING       ("dvd-ecc-pre-command",          0,  1,0,globalOptions.dvd.eccPreProcessCommand,                                                           "make DVD error-correction codes pre-process command","command"            ),
  CMD_OPTION_STRING       ("dvd-ecc-post-command",         0,  1,0,globalOptions.dvd.eccPostProcessCommand,                                                          "make DVD error-correction codes post-process command","command"           ),
  CMD_OPTION_STRING       ("dvd-ecc-command",              0,  1,0,globalOptions.dvd.eccCommand,                                                                     "make DVD error-correction codes command","command"                        ),
  CMD_OPTION_STRING       ("dvd-write-pre-command",        0,  1,0,globalOptions.dvd.writePreProcessCommand,                                                         "write DVD pre-process command","command"                                  ),
  CMD_OPTION_STRING       ("dvd-write-post-command",       0,  1,0,globalOptions.dvd.writePostProcessCommand,                                                        "write DVD post-process command","command"                                 ),
  CMD_OPTION_STRING       ("dvd-write-command",            0,  1,0,globalOptions.dvd.writeCommand,                                                                   "write DVD command","command"                                              ),
  CMD_OPTION_STRING       ("dvd-write-image-command",      0,  1,0,globalOptions.dvd.writeImageCommand,                                                              "write DVD image command","command"                                        ),

  CMD_OPTION_STRING       ("bd-request-volume-command",    0,  1,0,globalOptions.bd.requestVolumeCommand,                                                            "request new BD volume command","command"                                  ),
  CMD_OPTION_STRING       ("bd-unload-volume-command",     0,  1,0,globalOptions.bd.unloadVolumeCommand,                                                             "unload BD volume command","command"                                       ),
  CMD_OPTION_STRING       ("bd-load-volume-command",       0,  1,0,globalOptions.bd.loadVolumeCommand,                                                               "load BD volume command","command"                                         ),
  CMD_OPTION_INTEGER64    ("bd-volume-size",               0,  1,0,globalOptions.bd.volumeSize,              0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,             "BD volume size"                                                           ),
  CMD_OPTION_STRING       ("bd-image-pre-command",         0,  1,0,globalOptions.bd.imagePreProcessCommand,                                                          "make BD image pre-process command","command"                              ),
  CMD_OPTION_STRING       ("bd-image-post-command",        0,  1,0,globalOptions.bd.imagePostProcessCommand,                                                         "make BD image post-process command","command"                             ),
  CMD_OPTION_STRING       ("bd-image-command",             0,  1,0,globalOptions.bd.imageCommand,                                                                    "make BD image command","command"                                          ),
  CMD_OPTION_STRING       ("bd-ecc-pre-command",           0,  1,0,globalOptions.bd.eccPreProcessCommand,                                                            "make BD error-correction codes pre-process command","command"             ),
  CMD_OPTION_STRING       ("bd-ecc-post-command",          0,  1,0,globalOptions.bd.eccPostProcessCommand,                                                           "make BD error-correction codes post-process command","command"            ),
  CMD_OPTION_STRING       ("bd-ecc-command",               0,  1,0,globalOptions.bd.eccCommand,                                                                      "make BD error-correction codes command","command"                         ),
  CMD_OPTION_STRING       ("bd-write-pre-command",         0,  1,0,globalOptions.bd.writePreProcessCommand,                                                          "write BD pre-process command","command"                                   ),
  CMD_OPTION_STRING       ("bd-write-post-command",        0,  1,0,globalOptions.bd.writePostProcessCommand,                                                         "write BD post-process command","command"                                  ),
  CMD_OPTION_STRING       ("bd-write-command",             0,  1,0,globalOptions.bd.writeCommand,                                                                    "write BD command","command"                                               ),
  CMD_OPTION_STRING       ("bd-write-image-command",       0,  1,0,globalOptions.bd.writeImageCommand,                                                               "write BD image command","command"                                         ),

  CMD_OPTION_STRING       ("device",                       0,  1,0,globalOptions.defaultDeviceName,                                                                  "default device","device name"                                             ),
  CMD_OPTION_STRING       ("device-request-volume-command",0,  1,0,defaultDevice.requestVolumeCommand,                                                               "request new volume command","command"                                     ),
  CMD_OPTION_STRING       ("device-load-volume-command",   0,  1,0,defaultDevice.loadVolumeCommand,                                                                  "load volume command","command"                                            ),
  CMD_OPTION_STRING       ("device-unload-volume-command", 0,  1,0,defaultDevice.unloadVolumeCommand,                                                                "unload volume command","command"                                          ),
  CMD_OPTION_INTEGER64    ("device-volume-size",           0,  1,0,defaultDevice.volumeSize,                  0LL,MAX_LONG_LONG,COMMAND_LINE_BYTES_UNITS,             "volume size"                                                              ),
  CMD_OPTION_STRING       ("device-image-pre-command",     0,  1,0,defaultDevice.imagePreProcessCommand,                                                             "make image pre-process command","command"                                 ),
  CMD_OPTION_STRING       ("device-image-post-command",    0,  1,0,defaultDevice.imagePostProcessCommand,                                                            "make image post-process command","command"                                ),
  CMD_OPTION_STRING       ("device-image-command",         0,  1,0,defaultDevice.imageCommand,                                                                       "make image command","command"                                             ),
  CMD_OPTION_STRING       ("device-ecc-pre-command",       0,  1,0,defaultDevice.eccPreProcessCommand,                                                               "make error-correction codes pre-process command","command"                ),
  CMD_OPTION_STRING       ("device-ecc-post-command",      0,  1,0,defaultDevice.eccPostProcessCommand,                                                              "make error-correction codes post-process command","command"               ),
  CMD_OPTION_STRING       ("device-ecc-command",           0,  1,0,defaultDevice.eccCommand,                                                                         "make error-correction codes command","command"                            ),
  CMD_OPTION_STRING       ("device-write-pre-command",     0,  1,0,defaultDevice.writePreProcessCommand,                                                             "write device pre-process command","command"                               ),
  CMD_OPTION_STRING       ("device-write-post-command",    0,  1,0,defaultDevice.writePostProcessCommand,                                                            "write device post-process command","command"                              ),
  CMD_OPTION_STRING       ("device-write-command",         0,  1,0,defaultDevice.writeCommand,                                                                       "write device command","command"                                           ),

  CMD_OPTION_BOOLEAN      ("ecc",                          0,  1,1,jobOptions.errorCorrectionCodesFlag,                                                              "add error-correction codes with 'dvdisaster' tool"                        ),

  CMD_OPTION_CSTRING      ("database-file",                0,  1,0,indexDatabaseFileName,                                                                            "index database file name","file name"                                     ),
  CMD_OPTION_BOOLEAN      ("no-auto-update-database-index",0,  1,0,globalOptions.noAutoUpdateDatabaseIndexFlag,                                                      "disabled automatic update database index"                                 ),

  CMD_OPTION_SET          ("log",                          0,  1,0,logTypes,                                  COMMAND_LINE_OPTIONS_LOG_TYPES,                        "log types"                                                                ),
  CMD_OPTION_CSTRING      ("log-file",                     0,  1,0,logFileName,                                                                                      "log file name","file name"                                                ),
  CMD_OPTION_CSTRING      ("log-post-command",             0,  1,0,logPostCommand,                                                                                   "log file post-process command","command"                                  ),

  CMD_OPTION_CSTRING      ("pid-file",                     0,  1,0,pidFileName,                                                                                      "process id file name","file name"                                         ),

  CMD_OPTION_BOOLEAN      ("group",                        'g',0,0,globalOptions.groupFlag,                                                                          "group files in list"                                                      ),
  CMD_OPTION_BOOLEAN      ("all",                          0,  0,0,globalOptions.allFlag,                                                                            "show all files"                                                           ),
  CMD_OPTION_BOOLEAN      ("long-format",                  'L',0,0,globalOptions.longFormatFlag,                                                                     "list in long format"                                                      ),
  CMD_OPTION_BOOLEAN      ("human-format",                 'H',0,0,globalOptions.humanFormatFlag,                                                                    "list in human readable format"                                            ),
  CMD_OPTION_BOOLEAN      ("no-header-footer",             0,  0,0,globalOptions.noHeaderFooterFlag,                                                                 "output no header/footer in list"                                          ),
  CMD_OPTION_BOOLEAN      ("delete-old-archive-files",     0,  1,0,globalOptions.deleteOldArchiveFilesFlag,                                                          "delete old archive files after creating new files"                        ),
  CMD_OPTION_BOOLEAN      ("ignore-no-backup-file",        0,  1,1,globalOptions.ignoreNoBackupFileFlag,                                                             "ignore .nobackup/.NOBACKUP file"                                          ),

  CMD_OPTION_BOOLEAN      ("skip-unreadable",              0,  0,1,jobOptions.skipUnreadableFlag,                                                                    "skip unreadable files"                                                    ),
  CMD_OPTION_BOOLEAN      ("overwrite-archive-files",      'o',0,1,jobOptions.overwriteArchiveFilesFlag,                                                             "overwrite existing archive files"                                         ),
  CMD_OPTION_BOOLEAN      ("overwrite-files",              0,  0,1,jobOptions.overwriteFilesFlag,                                                                    "overwrite existing files"                                                 ),
  CMD_OPTION_BOOLEAN      ("wait-first-volume",            0,  1,1,jobOptions.waitFirstVolumeFlag,                                                                   "wait for first volume"                                                    ),
  CMD_OPTION_BOOLEAN      ("raw-images",                   0,  1,1,jobOptions.rawImagesFlag,                                                                         "store raw images (store all image blocks)"                                ),
  CMD_OPTION_BOOLEAN      ("dry-run",                      0,  1,1,jobOptions.dryRunFlag,                                                                            "do dry-run (skip storage/restore, incremental data, database index)"      ),
  CMD_OPTION_BOOLEAN      ("no-storage",                   0,  1,1,jobOptions.noStorageFlag,                                                                         "do not store archives (skip storage, database index)"                     ),
  CMD_OPTION_BOOLEAN      ("no-bar-on-medium",             0,  1,1,jobOptions.noBAROnMediumFlag,                                                                     "do not store a copy of BAR on medium"                                     ),
  CMD_OPTION_BOOLEAN      ("stop-on-error",                0,  1,1,jobOptions.stopOnErrorFlag,                                                                       "immediately stop on error"                                                ),

  CMD_OPTION_BOOLEAN      ("no-default-config",            0,  1,0,globalOptions.noDefaultConfigFlag,                                                                "do not read personal config file ~/.bar/" DEFAULT_CONFIG_FILE_NAME        ),
  CMD_OPTION_BOOLEAN      ("quiet",                        0,  1,0,globalOptions.quietFlag,                                                                          "suppress any output"                                                      ),
  CMD_OPTION_INTEGER_RANGE("verbose",                      'v',1,0,globalOptions.verboseLevel,                0,3,NULL,                                              "verbosity level"                                                          ),

  CMD_OPTION_BOOLEAN      ("version",                      0  ,0,0,versionFlag,                                                                                      "output version"                                                           ),
  CMD_OPTION_BOOLEAN      ("help",                         'h',0,0,helpFlag,                                                                                         "output this help"                                                         ),
  CMD_OPTION_BOOLEAN      ("xhelp",                        0,  0,0,xhelpFlag,                                                                                        "output help to extended options"                                          ),
  CMD_OPTION_BOOLEAN      ("help-internal",                0,  1,0,helpInternalFlag,                                                                                 "output help to internal options"                                          ),
};

LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value);

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

  {"all",       LOG_TYPE_ALL                },
};

LOCAL const ConfigValue CONFIG_VALUES[] =
{
  /* general settings */
  CONFIG_VALUE_SPECIAL  ("config",                       NULL,-1,                                                 configValueParseConfigFile,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_STRING   ("tmp-directory",                &globalOptions.tmpDirectory,-1                           ),
  CONFIG_VALUE_INTEGER64("max-tmp-size",                 &globalOptions.maxTmpSize,-1,                            0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER  ("nice-level",                   &globalOptions.niceLevel,-1,                             0,19,NULL),

  CONFIG_VALUE_INTEGER  ("max-band-width",               &globalOptions.maxBandWidth,-1,                          0,MAX_INT,CONFIG_VALUE_BITS_UNITS),

  CONFIG_VALUE_INTEGER  ("compress-min-size",            &globalOptions.compressMinFileSize,-1,                   0,MAX_INT,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_CSTRING  ("database-file",                &indexDatabaseFileName,-1                                ),
  CONFIG_VALUE_BOOLEAN  ("no-auto-update-database-index",&globalOptions.noAutoUpdateDatabaseIndexFlag,-1          ),

  /* global job settings */
  CONFIG_VALUE_STRING   ("archive-name",                 &archiveName,-1                                          ),
  CONFIG_VALUE_SELECT   ("archive-type",                 &jobOptions.archiveType,-1,                              CONFIG_VALUE_ARCHIVE_TYPES),

  CONFIG_VALUE_STRING   ("incremental-list-file",        &jobOptions.incrementalListFileName,-1                   ),

  CONFIG_VALUE_INTEGER64("archive-part-size",            &jobOptions.archivePartSize,-1,                          0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER  ("directory-strip",              &jobOptions.directoryStripCount,-1,                      0,MAX_INT,NULL),
  CONFIG_VALUE_STRING   ("destination",                  &jobOptions.destination,-1                               ),
  CONFIG_VALUE_SPECIAL  ("owner",                        &jobOptions.owner,-1,                                    configValueParseOwner,NULL,NULL,NULL,&jobOptions.owner),

  CONFIG_VALUE_SELECT   ("pattern-type",                 &jobOptions.patternType,-1,                              CONFIG_VALUE_PATTERN_TYPES),
 
  CONFIG_VALUE_SELECT   ("compress-algorithm",           &jobOptions.compressAlgorithm,-1,                        CONFIG_VALUE_COMPRESS_ALGORITHMS),

  CONFIG_VALUE_SELECT   ("crypt-algorithm",              &jobOptions.cryptAlgorithm,-1,                           CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_VALUE_SELECT   ("crypt-type",                   &jobOptions.cryptType,-1,                                CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_VALUE_SELECT   ("crypt-password-mode",          &jobOptions.cryptPasswordMode,-1,                        CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_VALUE_SPECIAL  ("crypt-password",               &globalOptions.cryptPassword,-1,                         configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_STRING   ("crypt-public-key",             &jobOptions.cryptPublicKeyFileName,-1                    ),
  CONFIG_VALUE_STRING   ("crypt-private-key",            &jobOptions.cryptPrivateKeyFileName,-1                   ),

  CONFIG_VALUE_STRING   ("ftp-login-name",               &currentFTPServer,offsetof(FTPServer,loginName)          ),
  CONFIG_VALUE_SPECIAL  ("ftp-password",                 &currentFTPServer,offsetof(FTPServer,password),          configValueParsePassword,NULL,NULL,NULL,NULL),

  CONFIG_VALUE_INTEGER  ("ssh-port",                     &currentSSHServer,offsetof(SSHServer,port),              0,65535,NULL),
  CONFIG_VALUE_STRING   ("ssh-login-name",               &currentSSHServer,offsetof(SSHServer,loginName)          ),
  CONFIG_VALUE_SPECIAL  ("ssh-password",                 &currentSSHServer,offsetof(SSHServer,password),          configValueParsePassword,NULL,NULL,NULL,NULL),
  CONFIG_VALUE_STRING   ("ssh-public-key",               &currentSSHServer,offsetof(SSHServer,publicKeyFileName)  ),
  CONFIG_VALUE_STRING   ("ssh-private-key",              &currentSSHServer,offsetof(SSHServer,privateKeyFileName) ),

  CONFIG_VALUE_SPECIAL  ("include-file",                 &includeEntryList,-1,                                    configValueParseFileEntry,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("include-image",                &includeEntryList,-1,                                    configValueParseImageEntry,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("exclude",                      &excludePatternList,-1,                                  configValueParseIncludeExclude,NULL,NULL,NULL,&jobOptions.patternType),
  CONFIG_VALUE_SPECIAL  ("exclude-compress",             &compressExcludePatternList,-1,                          configValueParseIncludeExclude,NULL,NULL,NULL,&jobOptions.patternType),

  CONFIG_VALUE_INTEGER64("volume-size",                  &jobOptions.volumeSize,-1,                               0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_BOOLEAN  ("ecc",                          &jobOptions.errorCorrectionCodesFlag,-1                  ),
 
  CONFIG_VALUE_BOOLEAN  ("skip-unreadable",              &jobOptions.skipUnreadableFlag,-1                        ),
  CONFIG_VALUE_BOOLEAN  ("raw-images",                   &jobOptions.rawImagesFlag,-1                             ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-archive-files",      &jobOptions.overwriteArchiveFilesFlag,-1                 ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-files",              &jobOptions.overwriteFilesFlag,-1                        ),
  CONFIG_VALUE_BOOLEAN  ("wait-first-volume",            &jobOptions.waitFirstVolumeFlag,-1                       ),
  CONFIG_VALUE_BOOLEAN  ("no-bar-on-medium",             &jobOptions.noBAROnMediumFlag,-1                         ),
  CONFIG_VALUE_BOOLEAN  ("quiet",                        &globalOptions.quietFlag,-1                              ),
  CONFIG_VALUE_INTEGER  ("verbose",                      &globalOptions.verboseLevel,-1,                          0,3,NULL),

  /* igored job settings (server only) */

  CONFIG_VALUE_SPECIAL  ("schedule",                     NULL,-1,                                                 configValueParseSchedule,configValueFormatInitSchedule,configValueFormatDoneSchedule,configValueFormatSchedule,NULL),

  /* commands */

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

  CONFIG_VALUE_STRING   ("device",                       &globalOptions.defaultDeviceName,-1                      ),
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

  /* server settings */

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
  long       nextIndex;

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
* Name   : cmdOptionParseOwner
* Purpose: command line option call back for parsing owner
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseOwner(void *userData, void *variable, const char *name, const char *value, const void *defaultValue)
{
  const char userName[256],groupName[256];
  uint32     userId,groupId;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  /* parse */
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

  /* store owner values */
  ((Owner*)variable)->userId  = userId;
  ((Owner*)variable)->groupId = groupId;

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

LOCAL bool cmdOptionParseEntry(void *userData, void *variable, const char *name, const char *value, const void *defaultValue)
{
  EntryTypes   entryType;
  PatternTypes patternType;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  /* get entry type */
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

  /* detect pattern type, get pattern */
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  /* append to list */
  if (EntryList_appendCString((EntryList*)variable,entryType,value,patternType) != ERROR_NONE)
  {
    fprintf(stderr,"Cannot parse varlue '%s' of option '%s'!\n",value,name);
    return FALSE;
  }

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

  globalOptions.runMode                     = RUN_MODE_INTERACTIVE;;
  globalOptions.barExecutable               = NULL;
  globalOptions.niceLevel                   = 0;
  globalOptions.tmpDirectory                = String_newCString(DEFAULT_TMP_DIRECTORY);
  globalOptions.maxTmpSize                  = 0LL;
  globalOptions.maxBandWidth                = 0;
  globalOptions.compressMinFileSize         = DEFAULT_COMPRESS_MIN_FILE_SIZE;
  globalOptions.cryptPassword               = NULL;
  globalOptions.ftpServer                   = globalOptions.defaultFTPServer;
  globalOptions.ftpServerList               = &ftpServerList;
  globalOptions.defaultFTPServer            = &defaultFTPServer;
  globalOptions.sshServer                   = globalOptions.defaultSSHServer;
  globalOptions.sshServerList               = &sshServerList;
  globalOptions.defaultSSHServer            = &defaultSSHServer;
  globalOptions.remoteBARExecutable         = NULL;

  globalOptions.cd.requestVolumeCommand     = NULL;
  globalOptions.cd.unloadVolumeCommand      = String_newCString(CD_UNLOAD_VOLUME_COMMAND);
  globalOptions.cd.loadVolumeCommand        = String_newCString(CD_LOAD_VOLUME_COMMAND);
  globalOptions.cd.volumeSize               = 0;
  globalOptions.cd.imagePreProcessCommand   = NULL;
  globalOptions.cd.imagePostProcessCommand  = NULL;
  globalOptions.cd.imageCommand             = String_newCString(CD_IMAGE_COMMAND);
  globalOptions.cd.eccPreProcessCommand     = NULL;
  globalOptions.cd.eccPostProcessCommand    = NULL;
  globalOptions.cd.eccCommand               = String_newCString(CD_ECC_COMMAND);
  globalOptions.cd.writePreProcessCommand   = NULL;
  globalOptions.cd.writePostProcessCommand  = NULL;
  globalOptions.cd.writeCommand             = String_newCString(CD_WRITE_COMMAND);
  globalOptions.cd.writeImageCommand        = String_newCString(CD_WRITE_IMAGE_COMMAND);

  globalOptions.dvd.requestVolumeCommand    = NULL;
  globalOptions.dvd.unloadVolumeCommand     = String_newCString(DVD_UNLOAD_VOLUME_COMMAND);
  globalOptions.dvd.loadVolumeCommand       = String_newCString(DVD_LOAD_VOLUME_COMMAND);
  globalOptions.dvd.volumeSize              = 0;
  globalOptions.dvd.imagePreProcessCommand  = NULL;
  globalOptions.dvd.imagePostProcessCommand = NULL;
  globalOptions.dvd.imageCommand            = String_newCString(DVD_IMAGE_COMMAND);
  globalOptions.dvd.eccPreProcessCommand    = NULL;
  globalOptions.dvd.eccPostProcessCommand   = NULL;
  globalOptions.dvd.eccCommand              = String_newCString(DVD_ECC_COMMAND);
  globalOptions.dvd.writePreProcessCommand  = NULL;
  globalOptions.dvd.writePostProcessCommand = NULL;
  globalOptions.dvd.writeCommand            = String_newCString(DVD_WRITE_COMMAND);
  globalOptions.dvd.writeImageCommand       = String_newCString(DVD_WRITE_IMAGE_COMMAND);

  globalOptions.bd.requestVolumeCommand     = NULL;
  globalOptions.bd.unloadVolumeCommand      = String_newCString(BD_UNLOAD_VOLUME_COMMAND);
  globalOptions.bd.loadVolumeCommand        = String_newCString(BD_LOAD_VOLUME_COMMAND);
  globalOptions.bd.volumeSize               = 0;
  globalOptions.bd.imagePreProcessCommand   = NULL;
  globalOptions.bd.imagePostProcessCommand  = NULL;
  globalOptions.bd.imageCommand             = String_newCString(BD_IMAGE_COMMAND);
  globalOptions.bd.eccPreProcessCommand     = NULL;
  globalOptions.bd.eccPostProcessCommand    = NULL;
  globalOptions.bd.eccCommand               = String_newCString(BD_ECC_COMMAND);
  globalOptions.bd.writePreProcessCommand   = NULL;
  globalOptions.bd.writePostProcessCommand  = NULL;
  globalOptions.bd.writeCommand             = String_newCString(BD_WRITE_COMMAND);
  globalOptions.bd.writeImageCommand        = String_newCString(BD_WRITE_IMAGE_COMMAND);

  globalOptions.defaultDeviceName           = String_newCString(DEFAULT_DEVICE_NAME);
  globalOptions.device                      = globalOptions.defaultDevice;
  globalOptions.deviceList                  = &deviceList;
  globalOptions.defaultDevice               = &defaultDevice;
  globalOptions.groupFlag                   = FALSE;
  globalOptions.allFlag                     = FALSE;
  globalOptions.longFormatFlag              = FALSE;
  globalOptions.noHeaderFooterFlag          = FALSE;
  globalOptions.deleteOldArchiveFilesFlag   = FALSE;
  globalOptions.noDefaultConfigFlag         = FALSE;
  globalOptions.quietFlag                   = FALSE;
  globalOptions.verboseLevel                = 1;
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
  error = Index_initAll();
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
  error = Network_initAll();
  if (error != ERROR_NONE)
  {
    Index_doneAll();
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
    Index_doneAll();
    Storage_doneAll();
    Archive_doneAll();
    PatternList_doneAll();
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }

  /* initialise variables */
  initGlobalOptions();

  command                               = COMMAND_LIST;
  jobName                               = NULL;
  archiveName                           = NULL;
  initJobOptions(&jobOptions);  
  List_init(&ftpServerList);
  List_init(&sshServerList);
  List_init(&deviceList);
  EntryList_init(&includeEntryList);
  PatternList_init(&excludePatternList);
  PatternList_init(&compressExcludePatternList);
  defaultFTPServer.loginName            = NULL;
  defaultFTPServer.password             = NULL;
  defaultSSHServer.port                 = 22;
  defaultSSHServer.loginName            = NULL;
  defaultSSHServer.password             = NULL;
  defaultSSHServer.publicKeyFileName    = NULL;
  defaultSSHServer.privateKeyFileName   = NULL;
  defaultDevice.requestVolumeCommand    = NULL;
  defaultDevice.unloadVolumeCommand     = NULL;
  defaultDevice.loadVolumeCommand       = NULL;
  defaultDevice.volumeSize              = 0LL;
  defaultDevice.imagePreProcessCommand  = NULL;
  defaultDevice.imagePostProcessCommand = NULL;
  defaultDevice.imageCommand            = NULL;
  defaultDevice.eccPreProcessCommand    = NULL;
  defaultDevice.eccPostProcessCommand   = NULL;
  defaultDevice.eccCommand              = NULL;
  defaultDevice.writePreProcessCommand  = NULL;
  defaultDevice.writePostProcessCommand = NULL;
  defaultDevice.writeCommand            = NULL;
  currentFTPServer                      = &defaultFTPServer;
  currentSSHServer                      = &defaultSSHServer;
  currentDevice                         = &defaultDevice;
  daemonFlag                            = FALSE;
  noDetachFlag                          = FALSE;
  serverPort                            = DEFAULT_SERVER_PORT;
  serverTLSPort                         = DEFAULT_TLS_SERVER_PORT;
  serverCAFileName                      = DEFAULT_TLS_SERVER_CA_FILE;
  serverCertFileName                    = DEFAULT_TLS_SERVER_CERTIFICATE_FILE;
  serverKeyFileName                     = DEFAULT_TLS_SERVER_KEY_FILE;
  serverPassword                        = Password_new();
  serverJobsDirectory                   = DEFAULT_JOBS_DIRECTORY;

  indexDatabaseFileName                 = NULL;

  logTypes                              = 0;
  logFileName                           = NULL;
  logPostCommand                        = NULL;

  batchFlag                             = FALSE;
  versionFlag                           = FALSE;
  helpFlag                              = FALSE;
  xhelpFlag                             = FALSE;
  helpInternalFlag                      = FALSE;

  pidFileName                           = NULL;

  keyFileName                           = NULL;
  keyBits                               = MIN_ASYMMETRIC_CRYPT_KEY_BITS;

  StringList_init(&configFileNameList);

  tmpDirectory                          = String_new();
  tmpLogFileName                        = String_new();
  logFile                               = NULL;
  tmpLogFile                            = NULL;

  outputLine                            = String_new();
  outputNewLineFlag                     = TRUE;

  /* initialize command line options and config values */
  ConfigValue_init(CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES));
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));

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
  /* deinitialize command line options and config values */
  CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  ConfigValue_done(CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES));

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
  PatternList_done(&compressExcludePatternList);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  Password_delete(serverPassword);
  List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
  List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
  List_done(&ftpServerList,(ListNodeFreeFunction)freeFTPServerNode,NULL);
  freeJobOptions(&jobOptions);
  String_delete(archiveName);
  String_delete(jobName);
  doneGlobalOptions();
  String_delete(outputLine);
  String_delete(tmpLogFileName);
  String_delete(tmpDirectory);
  StringList_done(&configFileNameList);
  String_delete(keyFileName);

  /* deinitialise modules */
  Server_doneAll();
  Network_doneAll();
  Index_doneAll();
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
        /* append to temporary log file */
        fprintf(tmpLogFile,"%s> ",String_cString(dateTime));
        if (prefix != NULL) (void)fputs(prefix,tmpLogFile);
        va_copy(tmpArguments,arguments);
        vfprintf(tmpLogFile,text,tmpArguments);
        va_end(tmpArguments);
        fprintf(tmpLogFile,"\n");
      }

      if (logFile != NULL)
      {
        /* append to log file */
        fprintf(logFile,"%s> ",String_cString(dateTime));
        if (prefix != NULL) (void)fputs(prefix,tmpLogFile);
        va_copy(tmpArguments,arguments);
        vfprintf(logFile,text,tmpArguments);
        va_end(tmpArguments);
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

  line = String_new();

  /* format line */
  va_start(arguments,text);
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

  line = String_new();

  /* format line */
  va_start(arguments,text);
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
  jobOptions->archiveType               = ARCHIVE_TYPE_NORMAL;
  jobOptions->archivePartSize           = 0LL;
  jobOptions->directoryStripCount       = 0;
  jobOptions->owner.userId              = FILE_DEFAULT_USER_ID;
  jobOptions->owner.groupId             = FILE_DEFAULT_GROUP_ID;
  jobOptions->patternType               = PATTERN_TYPE_GLOB;
  jobOptions->compressAlgorithm         = COMPRESS_ALGORITHM_NONE;
  jobOptions->cryptType                 = CRYPT_TYPE_NONE;
  jobOptions->cryptAlgorithm            = CRYPT_ALGORITHM_NONE;
  jobOptions->cryptPasswordMode         = PASSWORD_MODE_DEFAULT;
  jobOptions->volumeSize                = 0LL;
  jobOptions->skipUnreadableFlag        = TRUE;
  jobOptions->overwriteArchiveFilesFlag = FALSE;
  jobOptions->overwriteFilesFlag        = FALSE;
  jobOptions->errorCorrectionCodesFlag  = FALSE;
  jobOptions->waitFirstVolumeFlag       = FALSE;
  jobOptions->rawImagesFlag             = FALSE;
  jobOptions->dryRunFlag                = FALSE;
  jobOptions->noStorageFlag             = FALSE;
  jobOptions->noBAROnMediumFlag         = FALSE;
  jobOptions->stopOnErrorFlag           = FALSE;
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

void getFTPServerSettings(const String     name,
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

void getSSHServerSettings(const String     name,
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

void getCDSettings(const JobOptions *jobOptions,
                   OpticalDisk      *opticalDisk
                  )
{
  assert(jobOptions != NULL);
  assert(opticalDisk != NULL);

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
  Errors error;

  assert(password != NULL);
  assert(fileName != NULL);

  UNUSED_VARIABLE(userData);

  error = ERROR_UNKNOWN;
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

bool configValueParseOwner(void *userData, void *variable, const char *name, const char *value)
{
  const char userName[256],groupName[256];
  uint32     userId,groupId;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  /* parse */
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

  /* store owner values */
  ((Owner*)variable)->userId  = userId;
  ((Owner*)variable)->groupId = groupId;

  return TRUE;
}

void configValueFormatInitOwner(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (*(String**)variable);
}

void configValueFormatDoneOwner(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(formatUserData);  
}

bool configValueFormatOwner(void **formatUserData, void *userData, String line)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  if ((*formatUserData) != NULL)
  {
//???
    String_format(line,"%d:%d",0,0);

    (*formatUserData) = NULL;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

LOCAL bool configValueParseEntry(EntryTypes entryType, void *userData, void *variable, const char *name, const char *value)
{
  PatternTypes patternType;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
//??? userData = default patterType?
  UNUSED_VARIABLE(name);

  /* detect pattern type, get pattern */
  if      (strncmp(value,"r:",2) == 0) { patternType = PATTERN_TYPE_REGEX;          value += 2; }
  else if (strncmp(value,"x:",2) == 0) { patternType = PATTERN_TYPE_EXTENDED_REGEX; value += 2; }
  else if (strncmp(value,"g:",2) == 0) { patternType = PATTERN_TYPE_GLOB;           value += 2; }
  else                                 { patternType = PATTERN_TYPE_GLOB;                       }

  /* append to list */
  if (EntryList_appendCString((EntryList*)variable,entryType,value,patternType) != ERROR_NONE)
  {
    fprintf(stderr,"Cannot parse varlue '%s' of option '%s'!\n",value,name);
    return FALSE;
  }

  return TRUE;
}

bool configValueParseFileEntry(void *userData, void *variable, const char *name, const char *value)
{
  return configValueParseEntry(ENTRY_TYPE_FILE,userData,variable,name,value);
}

bool configValueParseImageEntry(void *userData, void *variable, const char *name, const char *value)
{
  return configValueParseEntry(ENTRY_TYPE_IMAGE,userData,variable,name,value);
}

void configValueFormatInitEntry(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = ((EntryList*)variable)->head;
}

void configValueFormatDoneEntry(void **formatUserData, void *userData)
{
  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(formatUserData);  
}

bool configValueFormatFileEntry(void **formatUserData, void *userData, String line)
{
  EntryNode *entryNode;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  entryNode = (EntryNode*)(*formatUserData);
  while ((entryNode != NULL) && (entryNode->type != ENTRY_TYPE_FILE))
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

/***********************************************************************\
* Name   : cmdOptionParseString
* Purpose: command line option call back for parsing string
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueParseString(void *userData, void *variable, const char *name, const char *value)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

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
  else if (String_equalsIgnoreCaseCString(name,"jan")) (*month) = MONTH_JAN;
  else if (String_equalsIgnoreCaseCString(name,"feb")) (*month) = MONTH_FEB;
  else if (String_equalsIgnoreCaseCString(name,"mar")) (*month) = MONTH_MAR;
  else if (String_equalsIgnoreCaseCString(name,"arp")) (*month) = MONTH_APR;
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
* Name   : parseScheduleWeekDays
* Purpose: parse week day
* Input  : s - string to parse
* Output : weekDays - week days
* Return : TRUE iff week day parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseScheduleWeekDays(const String s, ulong *weekDays)
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
  else if (String_equalsIgnoreCaseCString(name,"normal"     )) (*archiveType) = ARCHIVE_TYPE_NORMAL;
  else if (String_equalsIgnoreCaseCString(name,"full"       )) (*archiveType) = ARCHIVE_TYPE_FULL;
  else if (String_equalsIgnoreCaseCString(name,"incremental")) (*archiveType) = ARCHIVE_TYPE_INCREMENTAL;
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
  long         nextIndex;

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
  scheduleNode->weekDays    = SCHEDULE_ANY_DAY;
  scheduleNode->archiveType = ARCHIVE_TYPE_NORMAL;
  scheduleNode->enabled     = FALSE;

  /* parse schedule. Format: date [weekday] time enabled [type] */
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
    if (!parseScheduleWeekDays(s0,&scheduleNode->weekDays)) errorFlag = TRUE;
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

  /* initialise variables */
  line = String_new();

  /* open file */
  error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open file '%s' (error: %s)!\n",
               String_cString(fileName),
               Errors_getText(error)
              );
    String_delete(line);
    return FALSE;
  }

  /* parse file */
  failFlag = FALSE;
  lineNb   = 0;
  name     = String_new();
  value    = String_new();
  while (!File_eof(&fileHandle) && !failFlag)
  {
    /* read line */
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      printError("Cannot read file '%s' (error: %s)!\n",
                 String_cString(fileName),
                 Errors_getText(error)
                );
      failFlag = TRUE;
      break;
    }
    String_trim(line,STRING_WHITE_SPACES);
    lineNb++;

    /* skip comments, empty lines */
    if ((String_length(line) == 0) || (String_index(line,0) == '#'))
    {
      continue;
    }

    /* parse line */
    if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
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
//        failFlag = TRUE;
//        break;
      }
    }
    else
    {
      printError("Error in %s, line %ld: '%s' - skipped\n",
                 String_cString(fileName),
                 lineNb,
                 String_cString(line)
                );
//      failFlag = TRUE;
//      break;
    }
  }
  String_delete(value);
  String_delete(name);

  /* close file */
  File_close(&fileHandle);

  /* free resources */
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
  String         fileName;
  bool           printInfoFlag;
  Errors         error;
  DatabaseHandle databaseHandle;

  /* init */
  if (!initAll())
  {
    #ifndef NDEBUG
      Array_debugPrintInfo();
      Array_debugDone();
      String_debugPrintInfo();
      String_debugPrintStatistics();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_INIT_FAIL;
  }
  globalOptions.barExecutable = argv[0];

  /* parse command line: pre-options */
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,
                       stderr,"ERROR: "
                      )
     )
  {
    doneAll();
    #ifndef NDEBUG
      Array_debugPrintInfo();
      Array_debugDone();
      String_debugPrintInfo();
      String_debugPrintStatistics();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_INVALID_ARGUMENT;
  }

  if (!globalOptions.noDefaultConfigFlag)
  {
    fileName = File_newFileName();

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

    File_deleteFileName(fileName);
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
        Array_debugPrintInfo();
        Array_debugDone();
        String_debugPrintInfo();
        String_debugPrintStatistics();
        String_debugDone();
      #endif /* not NDEBUG */
      return EXITCODE_CONFIG_ERROR;
    }
  }
  String_delete(fileName);

  /* read job file */
  if (jobName != NULL)
  {
    fileName = File_newFileName();

    /* read job file */
    File_setFileNameCString(fileName,serverJobsDirectory);
    File_appendFileName(fileName,jobName);
    if (!readJobFile(fileName,
                     CONFIG_VALUES,
                     SIZE_OF_ARRAY(CONFIG_VALUES),
                     NULL
                    )
       )
    {
      File_deleteFileName(fileName);
      return EXITCODE_CONFIG_ERROR;
    }

    File_deleteFileName(fileName);
  }

  /* parse command line: all */
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       CMD_PRIORITY_ANY,
                       stderr,"ERROR: "
                      )
     )
  {
    doneAll();
    #ifndef NDEBUG
      Array_debugPrintInfo();
      Array_debugDone();
      String_debugPrintInfo();
      String_debugPrintStatistics();
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
      Array_debugPrintInfo();
      Array_debugDone();
      String_debugPrintInfo();
      String_debugPrintStatistics();
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
      Array_debugPrintInfo();
      Array_debugDone();
      String_debugPrintInfo();
      String_debugPrintStatistics();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_OK;
  }

  /* check parameters */
  if (!validateOptions())
  {
    doneAll();
    #ifndef NDEBUG
      Array_debugPrintInfo();
      Array_debugDone();
      String_debugPrintInfo();
      String_debugPrintStatistics();
      String_debugDone();
    #endif /* not NDEBUG */
    return EXITCODE_FAIL;
  }

  if (indexDatabaseFileName != NULL)
  {
    /* open index database */
    if (printInfoFlag) printf("Opening database file '%s'...",indexDatabaseFileName);
    error = Index_init(&databaseHandle,indexDatabaseFileName);
    if (error != ERROR_NONE)
    {
      if (printInfoFlag) printf("fail!\n");
      printError("Cannot open database '%s' (error: %s)!\n",
                 indexDatabaseFileName,
                 Errors_getText(error)
                );
      doneAll();
      #ifndef NDEBUG
        Array_debugPrintInfo();
        Array_debugDone();
        String_debugPrintInfo();
        String_debugPrintStatistics();
        String_debugDone();
      #endif /* not NDEBUG */
      return EXITCODE_FAIL;
    }

    indexDatabaseHandle = &databaseHandle;

    if (printInfoFlag) printf("ok\n");
  }
  else
  {
    /* no index database */
    indexDatabaseHandle = NULL;
  }

  /* create temporary directory */
  error = File_getTmpDirectoryNameCString(tmpDirectory,"bar-XXXXXX",globalOptions.tmpDirectory);
  if (error != ERROR_NONE)
  {
    printError("Cannot create temporary directory in '%s' (error: %s)!\n",String_cString(globalOptions.tmpDirectory),Errors_getText(error));
    doneAll();
    #ifndef NDEBUG
      Array_debugPrintInfo();
      Array_debugDone();
      String_debugPrintInfo();
      String_debugPrintStatistics();
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
          Array_debugPrintInfo();
          Array_debugDone();
          String_debugPrintInfo();
          String_debugPrintStatistics();
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
  else if (jobName != NULL)
  {
    globalOptions.runMode = RUN_MODE_INTERACTIVE;

    /* create archive */
    error = Command_create(String_cString(archiveName),
                           &includeEntryList,
                           &excludePatternList,
                           &compressExcludePatternList,
                           &jobOptions,
                           ARCHIVE_TYPE_NORMAL,
                           inputCryptPassword,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL
                          );

  }
  else
  {
    globalOptions.runMode = RUN_MODE_INTERACTIVE;

    switch (command)
    {
      case COMMAND_CREATE_FILES:
      case COMMAND_CREATE_IMAGES:
        {
          const char *archiveName;
          EntryTypes entryType;
          int        z;

          /* get archive file name */
          if (argc <= 1)
          {
            printError("No archive file name given!\n");
            error = ERROR_INVALID_ARGUMENT;
            break;
          }
          archiveName = argv[1];

          /* get include patterns */
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

          /* create archive */
          error = Command_create(archiveName,
                                 &includeEntryList,
                                 &excludePatternList,
                                 &compressExcludePatternList,
                                 &jobOptions,
                                 ARCHIVE_TYPE_NORMAL,
                                 inputCryptPassword,
                                 NULL,
                                 NULL,
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
          error = inputCryptPassword(NULL,&cryptPassword,privateKeyFileName,TRUE,FALSE);
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
          if (error != ERROR_NONE)
          {
            printError("Cannot create key pair (error: %s)!\n",Errors_getText(error));
            Crypt_doneKey(&privateKey);
            Crypt_doneKey(&publicKey);
            Password_done(&cryptPassword);
            File_deleteFileName(privateKeyFileName);
            File_deleteFileName(publicKeyFileName);
            break;
          }
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

  /* delete temporary directory */
  File_delete(tmpDirectory,TRUE);

  /* close index database (if open) */
  if (indexDatabaseHandle != NULL)
  {
    Database_close(indexDatabaseHandle);
  }

  /* free resources */
  doneAll();
  #ifndef NDEBUG
    Array_debugPrintInfo();
    Array_debugDone();
    String_debugPrintInfo();
    String_debugPrintStatistics();
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

