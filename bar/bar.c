/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.c,v $
* $Revision: 1.48 $
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
#include "patterns.h"
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
#define VERSION __VERSION_TO_STRING(VERSION_MAJOR) "." __VERSION_TO_STRING(VERSION_MINOR)

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

  COMMAND_UNKNOWN,
} Commands;

/***************************** Variables *******************************/
Options defaultOptions;

LOCAL Commands      command;
LOCAL const char    *archiveFileName;
LOCAL PatternList   includePatternList;
LOCAL PatternList   excludePatternList;
LOCAL SSHServer     sshServer;
LOCAL SSHServerList sshServerList;
LOCAL Device        device;
LOCAL DeviceList    deviceList;
LOCAL SSHServer     *currentSSHServer = &sshServer;
LOCAL Device        *currentDevice = &device;
LOCAL bool          daemonFlag;
LOCAL uint          serverPort;
LOCAL bool          serverTLSPort;
LOCAL const char    *serverCAFileName;
LOCAL const char    *serverCertFileName;
LOCAL const char    *serverKeyFileName;
LOCAL Password      *serverPassword;

LOCAL ulong         logTypes;
LOCAL const char    *logFileName;
LOCAL const char    *logPostCommand;

LOCAL bool          batchFlag;
LOCAL bool          versionFlag;
LOCAL bool          helpFlag,xhelpFlag,helpInternalFlag;

/*---------------------------------------------------------------------*/

LOCAL StringList    configFileNameList;

LOCAL String        tmpLogFileName;
LOCAL FILE          *logFile = NULL;
LOCAL FILE          *tmpLogFile = NULL;

LOCAL String        outputLine;
LOCAL bool          outputNewLineFlag;

/*---------------------------------------------------------------------*/

LOCAL bool cmdOptionParseString(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParseConfigFile(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParseIncludeExclude(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
LOCAL bool cmdOptionParsePassword(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);

const CommandLineUnit COMMAND_LINE_BYTES_UNITS[] =
{
  {"K",1024},
  {"M",1024*1024},
  {"G",1024*1024*1024},
};

const CommandLineUnit COMMAND_LINE_BITS_UNITS[] =
{
  {"K",1024},
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_PATTERN_TYPES[] =
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

  {"bzip1",COMPRESS_ALGORITHM_BZIP2_1,"BZIP2 compression level 1"},
  {"bzip2",COMPRESS_ALGORITHM_BZIP2_2,"BZIP2 compression level 2"},
  {"bzip3",COMPRESS_ALGORITHM_BZIP2_3,"BZIP2 compression level 3"},
  {"bzip4",COMPRESS_ALGORITHM_BZIP2_4,"BZIP2 compression level 4"},
  {"bzip5",COMPRESS_ALGORITHM_BZIP2_5,"BZIP2 compression level 5"},
  {"bzip6",COMPRESS_ALGORITHM_BZIP2_6,"BZIP2 compression level 6"},
  {"bzip7",COMPRESS_ALGORITHM_BZIP2_7,"BZIP2 compression level 7"},
  {"bzip8",COMPRESS_ALGORITHM_BZIP2_8,"BZIP2 compression level 8"},
  {"bzip9",COMPRESS_ALGORITHM_BZIP2_9,"BZIP2 compression level 9"},
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS[] =
{
  {"none",      CRYPT_ALGORITHM_NONE,      "no crypting"          },

  {"3DES",      CRYPT_ALGORITHM_3DES,      "3DES cipher"          },
  {"CAST5",     CRYPT_ALGORITHM_CAST5,     "CAST5 cipher"         },
  {"BLOWFISH",  CRYPT_ALGORITHM_BLOWFISH,  "Blowfish cipher"      },
  {"AES128",    CRYPT_ALGORITHM_AES128,    "AES cipher 128bit"    },
  {"AES192",    CRYPT_ALGORITHM_AES192,    "AES cipher 192bit"    },
  {"AES256",    CRYPT_ALGORITHM_AES256,    "AES cipher 256bit"    },
  {"TWOFISH128",CRYPT_ALGORITHM_TWOFISH128,"Twofish cipher 128bit"},
  {"TWOFISH256",CRYPT_ALGORITHM_TWOFISH256,"Twofish cipher 256bit"},
};

const CommandLineOptionSet COMMAND_LINE_OPTIONS_LOG_TYPES[] =
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
  CMD_OPTION_ENUM         ("create",                       'c',0,0,command,                                              COMMAND_NONE,COMMAND_CREATE,                                       "create new archive"                                                       ),
  CMD_OPTION_ENUM         ("list",                         'l',0,0,command,                                              COMMAND_NONE,COMMAND_LIST,                                         "list contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("test",                         't',0,0,command,                                              COMMAND_NONE,COMMAND_TEST,                                         "test contents of archive"                                                 ),
  CMD_OPTION_ENUM         ("compare",                      'd',0,0,command,                                              COMMAND_NONE,COMMAND_COMPARE,                                      "compare contents of archive with files"                                   ),
  CMD_OPTION_ENUM         ("extract",                      'x',0,0,command,                                              COMMAND_NONE,COMMAND_RESTORE,                                      "restore archive"                                                          ),

  CMD_OPTION_SPECIAL      ("config",                       0,  1,0,NULL,                                                 NULL,cmdOptionParseConfigFile,NULL,                                "configuration file","file name"                                           ),

  CMD_OPTION_INTEGER64    ("archive-part-size",            's',1,0,defaultOptions.archivePartSize,                       0,0,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                        "approximated archive part size"                                           ),
  CMD_OPTION_SPECIAL      ("tmp-directory",                0,  1,0,&defaultOptions.tmpDirectory,                         DEFAULT_TMP_DIRECTORY,cmdOptionParseString,NULL,                   "temporary directory","path"                                               ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",                 0,  1,0,defaultOptions.maxTmpSize,                            0,0,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                        "max. size of temporary files"                                             ),

  CMD_OPTION_ENUM         ("full",                         'f',0,0,defaultOptions.archiveType,                           ARCHIVE_TYPE_NORMAL,ARCHIVE_TYPE_FULL,                             "create full archive and incremental list file"                            ),
  CMD_OPTION_ENUM         ("incremental",                  'i',0,0,defaultOptions.archiveType,                           ARCHIVE_TYPE_NORMAL,ARCHIVE_TYPE_INCREMENTAL,                      "create incremental archive"                                               ),
  CMD_OPTION_SPECIAL      ("incremental-list-file",        0,  1,0,&defaultOptions.incrementalListFileName,              NULL,cmdOptionParseString,NULL,                                    "incremental list file name (implies --create-incremental-list","file name"),

  CMD_OPTION_INTEGER      ("directory-strip",              'p',1,0,defaultOptions.directoryStripCount,                   0,0,INT_MAX,NULL,                                                  "number of directories to strip on extract"                                ),
  CMD_OPTION_SPECIAL      ("directory",                    0,  0,0,&defaultOptions.directory   ,                         NULL,cmdOptionParseString,NULL,                                    "directory to restore files","path"                                        ),
  CMD_OPTION_INTEGER      ("nice-level",                   0,  1,0,defaultOptions.niceLevel,                             0,0,19,NULL,                                                       "general nice level of processes/threads"                                  ),

  CMD_OPTION_INTEGER      ("max-band-width",               0,  1,0,defaultOptions.maxBandWidth,                          0,0,INT_MAX,COMMAND_LINE_BITS_UNITS,                               "max. network band width to use"                                           ),

  CMD_OPTION_SELECT       ("pattern-type",                 0,  1,0,defaultOptions.patternType,                           PATTERN_TYPE_GLOB,COMMAND_LINE_OPTIONS_PATTERN_TYPES,              "select pattern type"                                                      ),

  CMD_OPTION_SPECIAL      ("include",                      '#',0,1,&includePatternList,                                  NULL,cmdOptionParseIncludeExclude,NULL,                            "include pattern","pattern"                                                ),
  CMD_OPTION_SPECIAL      ("exclude",                      '!',0,1,&excludePatternList,                                  NULL,cmdOptionParseIncludeExclude,NULL,                            "exclude pattern","pattern"                                                ),

  CMD_OPTION_SELECT       ("compress-algorithm",           'z',0,0,defaultOptions.compressAlgorithm,                     COMPRESS_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHMS,  "select compress algorithm to use"                                         ),
  CMD_OPTION_INTEGER      ("compress-min-size",            0,  1,0,defaultOptions.compressMinFileSize,                   DEFAULT_COMPRESS_MIN_FILE_SIZE,0,INT_MAX,COMMAND_LINE_BYTES_UNITS, "minimal size of file for compression"                                     ),

  CMD_OPTION_SELECT       ("crypt-algorithm",              'y',0,0,defaultOptions.cryptAlgorithm,                        CRYPT_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS,        "select crypt algorithm to use"                                            ),
  CMD_OPTION_SPECIAL      ("crypt-password",               0,  0,0,&defaultOptions.cryptPassword,                        NULL,cmdOptionParsePassword,NULL,                                  "crypt password (use with care!)","password"                               ),

  CMD_OPTION_INTEGER      ("ssh-port",                     0,  0,0,sshServer.port,                                       0,0,65535,NULL,                                                    "ssh port"                                                                 ),
  CMD_OPTION_SPECIAL      ("ssh-login-name",               0,  0,0,&sshServer.loginName,                                 NULL,cmdOptionParseString,NULL,                                    "ssh login name","name"                                                    ),
  CMD_OPTION_SPECIAL      ("ssh-public-key",               0,  1,0,&sshServer.publicKeyFileName,                         NULL,cmdOptionParseString,NULL,                                    "ssh public key file name","file name"                                     ),
  CMD_OPTION_SPECIAL      ("ssh-privat-key",               0,  1,0,&sshServer.privatKeyFileName,                         NULL,cmdOptionParseString,NULL,                                    "ssh privat key file name","file name"                                     ),
  CMD_OPTION_SPECIAL      ("ssh-password",                 0,  0,0,&sshServer.password,                                  NULL,cmdOptionParsePassword,NULL,                                  "ssh password (use with care!)","password"                                 ),

  CMD_OPTION_BOOLEAN      ("daemon",                       0,  1,0,daemonFlag,                                           FALSE,                                                             "run in daemon mode"                                                       ),
  CMD_OPTION_INTEGER      ("port",                         0,  1,0,serverPort,                                           DEFAULT_SERVER_PORT,0,65535,NULL,                                  "server port"                                                              ),
  CMD_OPTION_INTEGER      ("tls-port",                     0,  1,0,serverTLSPort,                                        DEFAULT_TLS_SERVER_PORT,0,65535,NULL,                              "TLS (SSL) server port"                                                    ),
  CMD_OPTION_STRING       ("server-ca-file",               0,  1,0,serverCAFileName,                                     DEFAULT_TLS_SERVER_CA_FILE,                                        "TLS (SSL) server certificate authority file (CA file)","file name"        ),
  CMD_OPTION_STRING       ("server-cert-file",             0,  1,0,serverCertFileName,                                   DEFAULT_TLS_SERVER_CERTIFICATE_FILE,                               "TLS (SSL) server certificate file","file name"                            ),
  CMD_OPTION_STRING       ("server-key-file",              0,  1,0,serverKeyFileName,                                    DEFAULT_TLS_SERVER_KEY_FILE,                                       "TLS (SSL) server key file","file name"                                    ),
  CMD_OPTION_SPECIAL      ("server-password",              0,  1,0,&serverPassword,                                      NULL,cmdOptionParsePassword,NULL,                                  "server password (use with care!)","password"                              ),

  CMD_OPTION_BOOLEAN      ("batch",                        0,  2,0,batchFlag,                                            FALSE,                                                             "run in batch mode"                                                        ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",        0,  1,0,&defaultOptions.remoteBARExecutable,                  DEFAULT_REMOTE_BAR_EXECUTABLE,cmdOptionParseString,NULL,           "remote BAR executable","file name"                                        ),

  CMD_OPTION_SPECIAL      ("dvd-request-volume-command",   0,  1,0,&defaultOptions.dvd.requestVolumeCommand,             NULL,cmdOptionParseString,NULL,                                    "request new DVD volume command","command"                                 ),
  CMD_OPTION_SPECIAL      ("dvd-unload-volume-command",    0,  1,0,&defaultOptions.dvd.unloadVolumeCommand,              DVD_UNLOAD_VOLUME_COMMAND,cmdOptionParseString,NULL,               "unload DVD volume command","command"                                      ),
  CMD_OPTION_SPECIAL      ("dvd-load-volume-command",      0,  1,0,&defaultOptions.dvd.loadVolumeCommand,                DVD_LOAD_VOLUME_COMMAND,cmdOptionParseString,NULL,                 "load DVD volume command","command"                                        ),
  CMD_OPTION_INTEGER64    ("dvd-volume-size",              0,  1,0,defaultOptions.dvd.volumeSize,                        0LL,0LL,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                    "DVD volume size"                                                          ),
  CMD_OPTION_SPECIAL      ("dvd-image-pre-command",        0,  1,0,&defaultOptions.dvd.imagePreProcessCommand,           NULL,cmdOptionParseString,NULL,                                    "make DVD image pre-process command","command"                             ),
  CMD_OPTION_SPECIAL      ("dvd-image-post-command",       0,  1,0,&defaultOptions.dvd.imagePostProcessCommand,          NULL,cmdOptionParseString,NULL,                                    "make DVD image post-process command","command"                            ),
  CMD_OPTION_SPECIAL      ("dvd-image-command",            0,  1,0,&defaultOptions.dvd.imageCommand,                     DVD_IMAGE_COMMAND,cmdOptionParseString,NULL,                       "make DVD image command","command"                                         ),
  CMD_OPTION_SPECIAL      ("dvd-ecc-pre-command",          0,  1,0,&defaultOptions.dvd.eccPreProcessCommand,             NULL,cmdOptionParseString,NULL,                                    "make DVD error-correction codes pre-process command","command"            ),
  CMD_OPTION_SPECIAL      ("dvd-ecc-post-command",         0,  1,0,&defaultOptions.dvd.eccPostProcessCommand,            NULL,cmdOptionParseString,NULL,                                    "make DVD error-correction codes post-process command","command"           ),
  CMD_OPTION_SPECIAL      ("dvd-ecc-command",              0,  1,0,&defaultOptions.dvd.eccCommand,                       DVD_ECC_COMMAND,cmdOptionParseString,NULL,                         "make DVD error-correction codes command","command"                        ),
  CMD_OPTION_SPECIAL      ("dvd-write-pre-command",        0,  1,0,&defaultOptions.dvd.writePreProcessCommand,           NULL,cmdOptionParseString,NULL,                                    "write DVD pre-process command","command"                                  ),
  CMD_OPTION_SPECIAL      ("dvd-write-post-command",       0,  1,0,&defaultOptions.dvd.writePostProcessCommand,          NULL,cmdOptionParseString,NULL,                                    "write DVD post-process command","command"                                 ),
  CMD_OPTION_SPECIAL      ("dvd-write-command",            0,  1,0,&defaultOptions.dvd.writeCommand,                     DVD_WRITE_COMMAND,cmdOptionParseString,NULL,                       "write DVD command","command"                                              ),
  CMD_OPTION_SPECIAL      ("dvd-write-command",            0,  1,0,&defaultOptions.dvd.writeImageCommand,                DVD_WRITE_IMAGE_COMMAND,cmdOptionParseString,NULL,                 "write DVD image command","command"                                        ),

  CMD_OPTION_SPECIAL      ("device",                       0,  1,0,&defaultOptions.defaultDeviceName,                    DEFAULT_DEVICE_NAME,cmdOptionParseString,NULL,                     "default device","device name"                                             ),
  CMD_OPTION_SPECIAL      ("device-request-volume-command",0,  1,0,&device.requestVolumeCommand,                         NULL,cmdOptionParseString,NULL,                                    "request new volume command","command"                                     ),
  CMD_OPTION_SPECIAL      ("device-unload-volume-command", 0,  1,0,&device.unloadVolumeCommand,                          NULL,cmdOptionParseString,NULL,                                    "unload volume command","command"                                          ),
  CMD_OPTION_SPECIAL      ("device-load-volume-command",   0,  1,0,&device.loadVolumeCommand,                            NULL,cmdOptionParseString,NULL,                                    "load volume command","command"                                            ),
  CMD_OPTION_INTEGER64    ("device-volume-size",           0,  1,0,device.volumeSize,                                    0LL,0LL,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                    "volume size"                                                              ),
  CMD_OPTION_SPECIAL      ("device-image-pre-command",     0,  1,0,&device.imagePreProcessCommand,                       NULL,cmdOptionParseString,NULL,                                    "make image pre-process command","command"                                 ),
  CMD_OPTION_SPECIAL      ("device-image-post-command",    0,  1,0,&device.imagePostProcessCommand,                      NULL,cmdOptionParseString,NULL,                                    "make image post-process command","command"                                ),
  CMD_OPTION_SPECIAL      ("device-image-command",         0,  1,0,&device.imageCommand,                                 NULL,cmdOptionParseString,NULL,                                    "make image command","command"                                             ),
  CMD_OPTION_SPECIAL      ("device-ecc-pre-command",       0,  1,0,&device.eccPreProcessCommand,                         NULL,cmdOptionParseString,NULL,                                    "make error-correction codes pre-process command","command"                ),
  CMD_OPTION_SPECIAL      ("device-ecc-post-command",      0,  1,0,&device.eccPostProcessCommand,                        NULL,cmdOptionParseString,NULL,                                    "make error-correction codes post-process command","command"               ),
  CMD_OPTION_SPECIAL      ("device-ecc-command",           0,  1,0,&device.eccCommand,                                   NULL,cmdOptionParseString,NULL,                                    "make error-correction codes command","command"                            ),
  CMD_OPTION_SPECIAL      ("device-write-pre-command",     0,  1,0,&device.writePreProcessCommand,                       NULL,cmdOptionParseString,NULL,                                    "write device pre-process command","command"                               ),
  CMD_OPTION_SPECIAL      ("device-write-post-command",    0,  1,0,&device.writePostProcessCommand,                      NULL,cmdOptionParseString,NULL,                                    "write device post-process command","command"                              ),
  CMD_OPTION_SPECIAL      ("device-write-command",         0,  1,0,&device.writeCommand,                                 NULL,cmdOptionParseString,NULL,                                    "write device command","command"                                           ),

  CMD_OPTION_BOOLEAN      ("ecc",                          0,  1,0,defaultOptions.errorCorrectionCodesFlag,              FALSE,                                                             "add error-correction codes with 'dvdisaster' tool"                        ),

  CMD_OPTION_SET          ("log",                          0,  1,0,logTypes,                                             0,COMMAND_LINE_OPTIONS_LOG_TYPES,                                  "log types"                                                                ),
  CMD_OPTION_STRING       ("log-file",                     0,  1,0,logFileName       ,                                   NULL,                                                              "log file name","file name"                                                ),
  CMD_OPTION_STRING       ("log-post-command",             0,  1,0,logPostCommand,                                       NULL,                                                              "log file post-process command","command"                                  ),

  CMD_OPTION_BOOLEAN      ("skip-unreadable",              0,  0,0,defaultOptions.skipUnreadableFlag,                    TRUE,                                                              "skip unreadable files"                                                    ),
  CMD_OPTION_BOOLEAN      ("overwrite-archive-files",      0,  0,0,defaultOptions.overwriteArchiveFilesFlag,             FALSE,                                                             "overwrite existing archive files"                                         ),
  CMD_OPTION_BOOLEAN      ("overwrite-files",              0,  0,0,defaultOptions.overwriteFilesFlag,                    FALSE,                                                             "overwrite existing files"                                                 ),
  CMD_OPTION_BOOLEAN      ("no-default-config",            0,  1,0,defaultOptions.noDefaultConfigFlag,                   FALSE,                                                             "do not read personal config file ~/.bar/" DEFAULT_CONFIG_FILE_NAME        ),
  CMD_OPTION_BOOLEAN      ("wait-first-volume",            0,  1,0,defaultOptions.waitFirstVolumeFlag,                   FALSE,                                                             "wait for first volume"                                                    ),
  CMD_OPTION_BOOLEAN      ("no-storage",                   0,  1,0,defaultOptions.noStorageFlag,                         FALSE,                                                             "do not store archives (skip storage)"                                     ),
  CMD_OPTION_BOOLEAN      ("no-bar-on-dvd",                0,  1,0,defaultOptions.noBAROnDVDFlag,                        FALSE,                                                             "do not store a copy of BAR on DVDs"                                       ),
  CMD_OPTION_BOOLEAN      ("stop-on-error",                0,  1,0,defaultOptions.stopOnErrorFlag,                       FALSE,                                                             "immediately stop on error"                                                ),
  CMD_OPTION_BOOLEAN      ("quiet",                        0,  1,0,defaultOptions.quietFlag,                             FALSE,                                                             "surpress any output"                                                      ),
  CMD_OPTION_INTEGER_RANGE("verbose",                      'v',1,0,defaultOptions.verboseLevel,                          1,0,3,NULL,                                                        "verbosity level"                                                          ),

  CMD_OPTION_BOOLEAN      ("version",                      0  ,0,0,versionFlag,                                          FALSE,                                                             "output version"                                                           ),
  CMD_OPTION_BOOLEAN      ("help",                         'h',0,0,helpFlag,                                             FALSE,                                                             "output this help"                                                         ),
  CMD_OPTION_BOOLEAN      ("xhelp",                        'h',0,0,xhelpFlag,                                            FALSE,                                                             "output help to extended options"                                          ),
  CMD_OPTION_BOOLEAN      ("help-internal",                'h',1,0,helpInternalFlag,                                     FALSE,                                                             "output help to internal options"                                          ),
};

/*---------------------------------------------------------------------*/

LOCAL bool configValueParseString(void *userData, void *variable, const char *name, const char *value);
LOCAL bool configValueParseConfigFile(void *userData, void *variable, const char *name, const char *value);
LOCAL bool configValueParseIncludeExclude(void *userData, void *variable, const char *name, const char *value);
LOCAL bool configValueParsePassword(void *userData, void *variable, const char *name, const char *value);

const ConfigValueUnit CONFIG_VALUE_BYTES_UNITS[] =
{
  {"K",1024},
  {"M",1024*1024},
  {"G",1024*1024*1024},
};

const ConfigValueUnit CONFIG_VALUE_BITS_UNITS[] =
{
  {"K",1024},
};

const ConfigValueSelect CONFIG_VALUE_PATTERN_TYPES[] =
{
  {"glob",    PATTERN_TYPE_GLOB,         },
  {"regex",   PATTERN_TYPE_REGEX,        },
  {"extended",PATTERN_TYPE_EXTENDED_REGEX},
};

const ConfigValueSelect CONFIG_VALUE_COMPRESS_ALGORITHMS[] =
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

  {"bzip1",COMPRESS_ALGORITHM_BZIP2_1},
  {"bzip2",COMPRESS_ALGORITHM_BZIP2_2},
  {"bzip3",COMPRESS_ALGORITHM_BZIP2_3},
  {"bzip4",COMPRESS_ALGORITHM_BZIP2_4},
  {"bzip5",COMPRESS_ALGORITHM_BZIP2_5},
  {"bzip6",COMPRESS_ALGORITHM_BZIP2_6},
  {"bzip7",COMPRESS_ALGORITHM_BZIP2_7},
  {"bzip8",COMPRESS_ALGORITHM_BZIP2_8},
  {"bzip9",COMPRESS_ALGORITHM_BZIP2_9},
};

const ConfigValueSelect CONFIG_VALUE_CRYPT_ALGORITHMS[] =
{
  {"none",      CRYPT_ALGORITHM_NONE,     },

  {"3DES",      CRYPT_ALGORITHM_3DES,     },
  {"CAST5",     CRYPT_ALGORITHM_CAST5,    },
  {"BLOWFISH",  CRYPT_ALGORITHM_BLOWFISH, },
  {"AES128",    CRYPT_ALGORITHM_AES128,   },
  {"AES192",    CRYPT_ALGORITHM_AES192,   },
  {"AES256",    CRYPT_ALGORITHM_AES256,   },
  {"TWOFISH128",CRYPT_ALGORITHM_TWOFISH128},
  {"TWOFISH256",CRYPT_ALGORITHM_TWOFISH256},
};

const ConfigValueSet CONFIG_VALUE_LOG_TYPES[] =
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
  CONFIG_VALUE_SPECIAL  ("config",                       NULL,-1,                                                 configValueParseConfigFile,NULL),

  CONFIG_VALUE_INTEGER64("archive-part-size",            defaultOptions.archivePartSize,-1,                       0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL  ("tmp-directory",                &defaultOptions.tmpDirectory,-1,                         configValueParseString,NULL),
  CONFIG_VALUE_INTEGER64("max-tmp-size",                 defaultOptions.maxTmpSize,-1,                            0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_INTEGER  ("directory-strip",              defaultOptions.directoryStripCount,-1,                   0,INT_MAX,NULL),
  CONFIG_VALUE_SPECIAL  ("directory",                    &defaultOptions.directory,-1,                            configValueParseString,NULL),
  CONFIG_VALUE_INTEGER  ("nice-level",                   defaultOptions.niceLevel,-1,                             0,19,NULL),

  CONFIG_VALUE_INTEGER  ("max-band-width",               defaultOptions.maxBandWidth,-1,                          0,INT_MAX,CONFIG_VALUE_BITS_UNITS),

  CONFIG_VALUE_SELECT   ("pattern-type",                 defaultOptions.patternType,-1,                           CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_VALUE_SPECIAL  ("include",                      &includePatternList,-1,                                  configValueParseIncludeExclude,NULL),
  CONFIG_VALUE_SPECIAL  ("exclude",                      &excludePatternList,-1,                                  configValueParseIncludeExclude,NULL),

  CONFIG_VALUE_SELECT   ("compress-algorithm",           defaultOptions.compressAlgorithm,-1,                     CONFIG_VALUE_COMPRESS_ALGORITHMS),
  CONFIG_VALUE_INTEGER  ("compress-min-size",            defaultOptions.compressMinFileSize,-1,                   0,INT_MAX,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_SELECT   ("crypt-algorithm",              defaultOptions.cryptAlgorithm,-1,                        CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_VALUE_SPECIAL  ("crypt-password",               &defaultOptions.cryptPassword,-1,                        configValueParsePassword,NULL),

  CONFIG_VALUE_INTEGER  ("ssh-port",                     currentSSHServer,offsetof(SSHServer,port),               0,65535,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-login-name",               &currentSSHServer,offsetof(SSHServer,loginName),         configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-public-key",               &currentSSHServer,offsetof(SSHServer,publicKeyFileName), configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-privat-key",               &currentSSHServer,offsetof(SSHServer,privatKeyFileName), configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-password",                 &currentSSHServer,offsetof(SSHServer,password),          configValueParsePassword,NULL),

  CONFIG_VALUE_INTEGER  ("port",                         serverPort,-1,                                           0,65535,NULL),
  CONFIG_VALUE_INTEGER  ("tls-port",                     serverTLSPort,-1,                                        0,65535,NULL),
  CONFIG_VALUE_STRING   ("server-ca-file",               serverCAFileName,-1                                      ),
  CONFIG_VALUE_STRING   ("server-cert-file",             serverCertFileName,-1                                    ),
  CONFIG_VALUE_STRING   ("server-key-file",              serverKeyFileName,-1                                     ),
  CONFIG_VALUE_SPECIAL  ("server-password",              &serverPassword,-1,                                      configValueParsePassword,NULL),

  CONFIG_VALUE_SPECIAL  ("remote-bar-executable",        &defaultOptions.remoteBARExecutable,-1,                  configValueParseString,NULL),

  CONFIG_VALUE_SPECIAL  ("dvd-request-volume-command",   &defaultOptions.dvd.requestVolumeCommand,-1,             configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-unload-volume-command",    &defaultOptions.dvd.unloadVolumeCommand,-1,              configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-load-volume-command",      &defaultOptions.dvd.loadVolumeCommand,-1,                configValueParseString,NULL),
  CONFIG_VALUE_INTEGER64("dvd-volume-size",              defaultOptions.dvd.volumeSize,-1,                        0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL  ("dvd-image-pre-command",        &defaultOptions.dvd.imagePreProcessCommand,-1,           configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-image-post-command",       &defaultOptions.dvd.imagePostProcessCommand,-1,          configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-image-command",            &defaultOptions.dvd.imageCommand,-1,                     configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-ecc-pre-command",          &defaultOptions.dvd.eccPreProcessCommand,-1,             configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-ecc-post-command",         &defaultOptions.dvd.eccPostProcessCommand,-1,            configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-ecc-command",              &defaultOptions.dvd.eccCommand,-1,                       configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-write-pre-command",        &defaultOptions.dvd.writePreProcessCommand,-1,           configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-write-post-command",       &defaultOptions.dvd.writePostProcessCommand,-1,          configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-write-command",            &defaultOptions.dvd.writeCommand,-1,                     configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("dvd-write-command",            &defaultOptions.dvd.writeImageCommand,-1,                configValueParseString,NULL),

  CONFIG_VALUE_SPECIAL  ("device",                       &defaultOptions.defaultDeviceName,-1,                    configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-request-volume-command",&currentDevice,offsetof(Device,requestVolumeCommand),    configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-unload-volume-command", &currentDevice,offsetof(Device,unloadVolumeCommand),     configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-load-volume-command",   &currentDevice,offsetof(Device,loadVolumeCommand),       configValueParseString,NULL),
  CONFIG_VALUE_INTEGER64("device-volume-size",           currentDevice,offsetof(Device,volumeSize),               0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL  ("device-image-pre-command",     &currentDevice,offsetof(Device,imagePreProcessCommand),  configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-image-post-command",    &currentDevice,offsetof(Device,imagePostProcessCommand), configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-image-command",         &currentDevice,offsetof(Device,imageCommand),            configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-ecc-pre-command",       &currentDevice,offsetof(Device,eccPreProcessCommand),    configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-ecc-post-command",      &currentDevice,offsetof(Device,eccPostProcessCommand),   configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-ecc-command",           &currentDevice,offsetof(Device,eccCommand),              configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-write-pre-command",     &currentDevice,offsetof(Device,writePreProcessCommand),  configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-write-post-command",    &currentDevice,offsetof(Device,writePostProcessCommand), configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-write-command",         &currentDevice,offsetof(Device,writeCommand),            configValueParseString,NULL),

  CONFIG_VALUE_BOOLEAN  ("ecc",                          defaultOptions.errorCorrectionCodesFlag,-1               ),

  CONFIG_VALUE_SET      ("log",                          logTypes,-1,                                             CONFIG_VALUE_LOG_TYPES),
  CONFIG_VALUE_STRING   ("log-file",                     logFileName,-1                                           ),
  CONFIG_VALUE_STRING   ("log-post-command",             logPostCommand,-1                                        ),

  CONFIG_VALUE_BOOLEAN  ("skip-unreadable",              defaultOptions.skipUnreadableFlag,-1                     ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-archive-files",      defaultOptions.overwriteArchiveFilesFlag,-1              ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-files",              defaultOptions.overwriteFilesFlag,-1                     ),
  CONFIG_VALUE_BOOLEAN  ("wait-first-volume",            defaultOptions.waitFirstVolumeFlag,-1                    ),
  CONFIG_VALUE_BOOLEAN  ("no-bar-on-dvd",                defaultOptions.noBAROnDVDFlag,-1                         ),
  CONFIG_VALUE_BOOLEAN  ("quiet",                        defaultOptions.quietFlag,-1                              ),
  CONFIG_VALUE_INTEGER  ("verbose",                      defaultOptions.verboseLevel,-1,                          0,3,NULL),
};

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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
* Input  : fileName - file name
* Output : -
* Return : TRUE iff configuration read, FALSE otherwise (error)
* Notes  : -
\***********************************************************************/

LOCAL bool readConfigFile(String fileName, bool printInfoFlag)
{
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     name,value;

  assert(fileName != NULL);

  /* open file */
  error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open file '%s' (error: %s)!\n",
               String_cString(fileName),
               getErrorText(error)
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
                 getErrorText(error)
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
    if      (String_parse(line,"[server %S]",NULL,name))
    {
      SSHServerNode *sshServerNode;

      sshServerNode = LIST_NEW_NODE(SSHServerNode);
      if (sshServerNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      sshServerNode->name                        = String_duplicate(name);
      sshServerNode->sshServer.port              = 0;
      sshServerNode->sshServer.loginName         = NULL;
      sshServerNode->sshServer.publicKeyFileName = NULL;
      sshServerNode->sshServer.privatKeyFileName = NULL;
      sshServerNode->sshServer.password          = NULL;

      List_append(&sshServerList,sshServerNode);

      currentSSHServer = &sshServerNode->sshServer;
    }
    else if (String_parse(line,"[device %S]",NULL,name))
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
    else if (String_parse(line,"[global]",NULL))
    {
      currentSSHServer = &sshServer;
      currentDevice    = &device;
    }
    else if (String_parse(line,"%S=%S",NULL,name,value))
    {
      if (!ConfigValue_parse(String_cString(name),
                             String_cString(value),
                             CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES),
                             NULL,
                             NULL
                            )
         )
      {
        if (printInfoFlag) printf("FAIL!\n");
        printError("Unknown or invalid config value '%s' in %s, line %ld\n",
                   String_cString(name),
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
      printError("Error in %s, line %ld: %s\n",
                 String_cString(fileName),
                 lineNb,
                 String_cString(line)
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
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  if (Pattern_appendList((PatternList*)variable,value,defaultOptions.patternType) != ERROR_NONE)
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

  if ((*(Password**)variable) != NULL)
  {
    Password_setCString(*(Password**)variable,value);
  }
  else
  {
    (*(Password**)variable) = Password_newCString(value);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseString
* Purpose: command line option call back for parsing string
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueParseString(void *userData, void *variable, const char *name, const char *value)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(name);
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
* Name   : configValueParseIncludeExclude
* Purpose: command line option call back for parsing include/exclude
*          patterns
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueParseIncludeExclude(void *userData, void *variable, const char *name, const char *value)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);

  if (Pattern_appendList((PatternList*)variable,value,defaultOptions.patternType) != ERROR_NONE)
  {
    fprintf(stderr,"Cannot parse varlue '%s' of option '%s'!\n",value,name);
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParsePassword
* Purpose: command line option call back for parsing password
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueParsePassword(void *userData, void *variable, const char *name, const char *value)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(userData);

  if ((*(Password**)variable) != NULL)
  {
    Password_setCString(*(Password**)variable,value);
  }
  else
  {
    (*(Password**)variable) = Password_newCString(value);
  }

  return TRUE;
}

/***********************************************************************\
* Name       : printUsage
* Purpose    : print "usage" help
* Input      : -
* Output     : -
* Return     : -
* Side-effect: unknown
* Notes      : -
\***********************************************************************/

LOCAL void printUsage(const char *programName, uint level)
{
  assert(programName != NULL);

  printf("Usage: %s [<options>] [--] <archive name>|scp:<name>@<host name>:<archive name>... [<files>...]\n",programName);
  printf("\n");
  CmdOption_printHelp(stdout,
                      COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                      level
                     );
}

LOCAL void initOptions(Options *options)
{
  assert(options != NULL);

  memset(options,0,sizeof(Options));
  options->sshServerList = &sshServerList;
  options->sshServer     = &options->defaultSSHServer;
  options->deviceList    = &deviceList;
  options->device        = &options->defaultDevice;
}

LOCAL void freeSSHServerNode(SSHServerNode *sshServerNode, void *userData)
{
  assert(sshServerNode != NULL);

  UNUSED_VARIABLE(userData);

  Password_delete(sshServerNode->sshServer.password);
  String_delete(sshServerNode->sshServer.privatKeyFileName);
  String_delete(sshServerNode->sshServer.publicKeyFileName);
  String_delete(sshServerNode->sshServer.loginName        );
  String_delete(sshServerNode->name);
}

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
  error = Archive_initAll();
  if (error != ERROR_NONE)
  {
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }
  error = Storage_initAll();
  if (error != ERROR_NONE)
  {
    Archive_doneAll();
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
    Pattern_doneAll();
    Crypt_doneAll();
    Password_doneAll();
    return FALSE;
  }

  /* initialise variables */
  StringList_init(&configFileNameList);
  tmpLogFileName = String_new();
  outputLine = String_new();
  outputNewLineFlag = TRUE;
  initOptions(&defaultOptions);
  serverPassword = Password_new();
  Pattern_initList(&includePatternList);
  Pattern_initList(&excludePatternList);
  List_init(&sshServerList);
  List_init(&deviceList);

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
  List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
  List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
  Pattern_doneList(&excludePatternList);
  Pattern_doneList(&includePatternList);
  Password_delete(serverPassword);
  freeOptions(&defaultOptions);
  String_delete(outputLine);
  String_delete(tmpLogFileName);
  StringList_done(&configFileNameList);

  /* deinitialise modules */
  Server_doneAll();
  Network_doneAll();
  Storage_doneAll();
  Archive_doneAll();
  Pattern_doneAll();
  Crypt_doneAll();
  Password_doneAll();
}

/*---------------------------------------------------------------------*/

void vprintInfo(uint verboseLevel, const char *prefix, const char *format, va_list arguments)
{
  String line;

  assert(format != NULL);

  if (!defaultOptions.quietFlag && (defaultOptions.verboseLevel >= verboseLevel))
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
  char dateTime[32];

  assert(text != NULL);

  if ((logType == LOG_TYPE_ALWAYS) || ((logTypes & logType) != 0))
  {
    Misc_getDateTime(dateTime,sizeof(dateTime));

    if (tmpLogFile != NULL)
    {
      /* append to temporary log file */
      fprintf(tmpLogFile,"%s> ",dateTime);
      if (prefix != NULL) fprintf(tmpLogFile,prefix);
      vfprintf(tmpLogFile,text,arguments);
      fprintf(tmpLogFile,"\n");
    }

    if (logFile != NULL)
    {
      /* append to log file */
      fprintf(logFile,"%s> ",dateTime);
      if (prefix != NULL) fprintf(logFile,prefix);
      vfprintf(logFile,text,arguments);
      fprintf(logFile,"\n");
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
  String  line;
  va_list arguments;

  assert(text != NULL);

  line = String_new();

  /* format line */
  va_start(arguments,text);
  vlogMessage(LOG_TYPE_WARNING,"Warning: ",text,arguments);
  String_appendCString(line,"Warning: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  /* output */
  output(stdout,TRUE,line);

  String_delete(line);
}

void printError(const char *text, ...)
{
  String  line;
  va_list arguments;

  assert(text != NULL);

  line = String_new();

  /* format line */
  va_start(arguments,text);
  vlogMessage(LOG_TYPE_ERROR,"ERROR: ",text,arguments);
  String_appendCString(line,"ERROR: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  /* output */
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
    TEXT_MACRO_STRING(textMacros[0],"%file",tmpLogFileName);
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
      printError("Cannot post-process log file (error: %s)\n",getErrorText(error));
    }
  }

  /* reset temporary log file */
  if (tmpLogFile != NULL) fclose(tmpLogFile);
  tmpLogFile = fopen(String_cString(tmpLogFileName),"w");
}

void copyOptions(Options *destinationOptions, const Options *sourceOptions)
{
  assert(sourceOptions != NULL);
  assert(destinationOptions != NULL);

  memcpy(destinationOptions,sourceOptions,sizeof(Options));
  destinationOptions->tmpDirectory                          = String_duplicate(sourceOptions->tmpDirectory);
  destinationOptions->incrementalListFileName               = String_duplicate(sourceOptions->incrementalListFileName);
  destinationOptions->directory                             = String_duplicate(sourceOptions->directory);
  destinationOptions->cryptPassword                         = Password_duplicate(sourceOptions->cryptPassword);
  destinationOptions->sshServer                             = &destinationOptions->defaultSSHServer;
  destinationOptions->defaultSSHServer.loginName            = String_duplicate(sourceOptions->defaultSSHServer.loginName);
  destinationOptions->defaultSSHServer.publicKeyFileName    = String_duplicate(sourceOptions->defaultSSHServer.publicKeyFileName);
  destinationOptions->defaultSSHServer.privatKeyFileName    = String_duplicate(sourceOptions->defaultSSHServer.privatKeyFileName);
  destinationOptions->defaultSSHServer.password             = Password_duplicate(sourceOptions->defaultSSHServer.password);
  destinationOptions->defaultDeviceName                     = String_duplicate(sourceOptions->defaultDeviceName);
  destinationOptions->dvd.requestVolumeCommand              = String_duplicate(sourceOptions->dvd.requestVolumeCommand);
  destinationOptions->dvd.unloadVolumeCommand               = String_duplicate(sourceOptions->dvd.unloadVolumeCommand);
  destinationOptions->dvd.loadVolumeCommand                 = String_duplicate(sourceOptions->dvd.loadVolumeCommand);
  destinationOptions->dvd.imagePreProcessCommand            = String_duplicate(sourceOptions->dvd.imagePreProcessCommand);
  destinationOptions->dvd.imagePostProcessCommand           = String_duplicate(sourceOptions->dvd.imagePostProcessCommand);
  destinationOptions->dvd.imageCommand                      = String_duplicate(sourceOptions->dvd.imageCommand);
  destinationOptions->dvd.eccPreProcessCommand              = String_duplicate(sourceOptions->dvd.eccPreProcessCommand);
  destinationOptions->dvd.eccPostProcessCommand             = String_duplicate(sourceOptions->dvd.eccPostProcessCommand);
  destinationOptions->dvd.eccCommand                        = String_duplicate(sourceOptions->dvd.eccCommand);
  destinationOptions->dvd.writePreProcessCommand            = String_duplicate(sourceOptions->dvd.writePreProcessCommand);
  destinationOptions->dvd.writePostProcessCommand           = String_duplicate(sourceOptions->dvd.writePostProcessCommand);
  destinationOptions->dvd.writeCommand                      = String_duplicate(sourceOptions->dvd.writeCommand);
  destinationOptions->dvd.writeImageCommand                 = String_duplicate(sourceOptions->dvd.writeImageCommand);
  destinationOptions->device                                = &destinationOptions->defaultDevice;
  destinationOptions->defaultDevice.requestVolumeCommand    = String_duplicate(sourceOptions->defaultDevice.requestVolumeCommand);
  destinationOptions->defaultDevice.unloadVolumeCommand     = String_duplicate(sourceOptions->defaultDevice.unloadVolumeCommand);
  destinationOptions->defaultDevice.loadVolumeCommand       = String_duplicate(sourceOptions->defaultDevice.loadVolumeCommand);
  destinationOptions->defaultDevice.imagePreProcessCommand  = String_duplicate(sourceOptions->defaultDevice.imagePreProcessCommand);
  destinationOptions->defaultDevice.imagePostProcessCommand = String_duplicate(sourceOptions->defaultDevice.imagePostProcessCommand);
  destinationOptions->defaultDevice.imageCommand            = String_duplicate(sourceOptions->defaultDevice.imageCommand);
  destinationOptions->defaultDevice.eccPreProcessCommand    = String_duplicate(sourceOptions->defaultDevice.eccPreProcessCommand);
  destinationOptions->defaultDevice.eccPostProcessCommand   = String_duplicate(sourceOptions->defaultDevice.eccPostProcessCommand);
  destinationOptions->defaultDevice.eccCommand              = String_duplicate(sourceOptions->defaultDevice.eccCommand);
  destinationOptions->defaultDevice.writePreProcessCommand  = String_duplicate(sourceOptions->defaultDevice.writePreProcessCommand);
  destinationOptions->defaultDevice.writePostProcessCommand = String_duplicate(sourceOptions->defaultDevice.writePostProcessCommand);
  destinationOptions->defaultDevice.writeCommand            = String_duplicate(sourceOptions->defaultDevice.writeCommand);
  destinationOptions->remoteBARExecutable                   = String_duplicate(sourceOptions->remoteBARExecutable);
}

void freeOptions(Options *options)
{
  assert(options != NULL);

  String_delete(options->remoteBARExecutable);
  String_delete(options->defaultDevice.writeCommand);
  String_delete(options->defaultDevice.writePostProcessCommand);
  String_delete(options->defaultDevice.writePreProcessCommand);
  String_delete(options->defaultDevice.eccCommand);
  String_delete(options->defaultDevice.eccPostProcessCommand);
  String_delete(options->defaultDevice.eccPreProcessCommand);
  String_delete(options->defaultDevice.imageCommand);
  String_delete(options->defaultDevice.imagePostProcessCommand);
  String_delete(options->defaultDevice.imagePreProcessCommand);
  String_delete(options->defaultDevice.loadVolumeCommand);
  String_delete(options->defaultDevice.unloadVolumeCommand);
  String_delete(options->defaultDevice.requestVolumeCommand);
  String_delete(options->defaultDeviceName);
  String_delete(options->dvd.writeImageCommand);
  String_delete(options->dvd.writeCommand);
  String_delete(options->dvd.writePostProcessCommand);
  String_delete(options->dvd.writePreProcessCommand);
  String_delete(options->dvd.eccCommand);
  String_delete(options->dvd.eccPostProcessCommand);
  String_delete(options->dvd.eccPreProcessCommand);
  String_delete(options->dvd.imageCommand);
  String_delete(options->dvd.imagePostProcessCommand);
  String_delete(options->dvd.imagePreProcessCommand);
  String_delete(options->dvd.loadVolumeCommand);
  String_delete(options->dvd.unloadVolumeCommand);
  String_delete(options->dvd.requestVolumeCommand);
  Password_delete(options->defaultSSHServer.password);
  String_delete(options->defaultSSHServer.privatKeyFileName);
  String_delete(options->defaultSSHServer.publicKeyFileName);
  String_delete(options->defaultSSHServer.loginName);
  Password_delete(options->cryptPassword);
  String_delete(options->directory);
  String_delete(options->incrementalListFileName);
  String_delete(options->tmpDirectory);
  memset(options,0,sizeof(Options));
}

void getSSHServer(const String  name,
                  const Options *options,
                  SSHServer     *sshServer
                 )
{
  SSHServerNode *sshServerNode;

  assert(name != NULL);
  assert(options != NULL);
  assert(sshServer != NULL);

  sshServerNode = options->sshServerList->head;
  while ((sshServerNode != NULL) && !String_equals(sshServerNode->name,name))
  {
    sshServerNode = sshServerNode->next;
  }
  sshServer->port              = (options->sshServer->port              != 0)?options->sshServer->port             :((sshServerNode != NULL)?sshServerNode->sshServer.port             :options->defaultSSHServer.port             );
  sshServer->loginName         = (options->sshServer->loginName         != 0)?options->sshServer->loginName        :((sshServerNode != NULL)?sshServerNode->sshServer.loginName        :options->defaultSSHServer.loginName        );
  sshServer->publicKeyFileName = (options->sshServer->publicKeyFileName != 0)?options->sshServer->publicKeyFileName:((sshServerNode != NULL)?sshServerNode->sshServer.publicKeyFileName:options->defaultSSHServer.publicKeyFileName);
  sshServer->privatKeyFileName = (options->sshServer->privatKeyFileName != 0)?options->sshServer->privatKeyFileName:((sshServerNode != NULL)?sshServerNode->sshServer.privatKeyFileName:options->defaultSSHServer.privatKeyFileName);
  sshServer->password          = (options->sshServer->password          != 0)?options->sshServer->password         :((sshServerNode != NULL)?sshServerNode->sshServer.password         :options->defaultSSHServer.password         );
}

void getDevice(const String  name,
               const Options *options,
               Device        *device
              )
{
  DeviceNode *deviceNode;

  assert(name != NULL);
  assert(options != NULL);
  assert(device != NULL);

  deviceNode = options->deviceList->head;
  while ((deviceNode != NULL) && !String_equals(deviceNode->name,name))
  {
    deviceNode = deviceNode->next;
  }
  device->requestVolumeCommand    = (options->device->requestVolumeCommand    != 0)?options->device->requestVolumeCommand   :((deviceNode != NULL)?deviceNode->device.requestVolumeCommand   :options->defaultDevice.requestVolumeCommand   );
  device->unloadVolumeCommand     = (options->device->unloadVolumeCommand     != 0)?options->device->unloadVolumeCommand    :((deviceNode != NULL)?deviceNode->device.unloadVolumeCommand    :options->defaultDevice.unloadVolumeCommand    );
  device->loadVolumeCommand       = (options->device->loadVolumeCommand       != 0)?options->device->loadVolumeCommand      :((deviceNode != NULL)?deviceNode->device.loadVolumeCommand      :options->defaultDevice.loadVolumeCommand      );
  device->volumeSize              = (options->device->volumeSize              != 0)?options->device->volumeSize             :((deviceNode != NULL)?deviceNode->device.volumeSize             :options->defaultDevice.volumeSize             );
  device->imagePreProcessCommand  = (options->device->imagePreProcessCommand  != 0)?options->device->imagePreProcessCommand :((deviceNode != NULL)?deviceNode->device.imagePreProcessCommand :options->defaultDevice.imagePreProcessCommand );
  device->imagePostProcessCommand = (options->device->imagePostProcessCommand != 0)?options->device->imagePostProcessCommand:((deviceNode != NULL)?deviceNode->device.imagePostProcessCommand:options->defaultDevice.imagePostProcessCommand);
  device->imageCommand            = (options->device->imageCommand            != 0)?options->device->imageCommand           :((deviceNode != NULL)?deviceNode->device.imageCommand           :options->defaultDevice.imageCommand           );
  device->eccPreProcessCommand    = (options->device->eccPreProcessCommand    != 0)?options->device->eccPreProcessCommand   :((deviceNode != NULL)?deviceNode->device.eccPreProcessCommand   :options->defaultDevice.eccPreProcessCommand   );
  device->eccPostProcessCommand   = (options->device->eccPostProcessCommand   != 0)?options->device->eccPostProcessCommand  :((deviceNode != NULL)?deviceNode->device.eccPostProcessCommand  :options->defaultDevice.eccPostProcessCommand  );
  device->eccCommand              = (options->device->eccCommand              != 0)?options->device->eccCommand             :((deviceNode != NULL)?deviceNode->device.eccCommand             :options->defaultDevice.eccCommand             );
  device->writePreProcessCommand  = (options->device->writePreProcessCommand  != 0)?options->device->writePreProcessCommand :((deviceNode != NULL)?deviceNode->device.writePreProcessCommand :options->defaultDevice.writePreProcessCommand );
  device->writePostProcessCommand = (options->device->writePostProcessCommand != 0)?options->device->writePostProcessCommand:((deviceNode != NULL)?deviceNode->device.writePostProcessCommand:options->defaultDevice.writePostProcessCommand);
  device->writeCommand            = (options->device->writeCommand            != 0)?options->device->writeCommand           :((deviceNode != NULL)?deviceNode->device.writeCommand           :options->defaultDevice.writeCommand           );
}

bool inputCryptPassword(Password **cryptPassword)
{
  Password *password;

  password = (*cryptPassword);
  if (password == NULL)
  {
    /* allocate password */
    password = Password_new();
    if (password == NULL)
    {
      return FALSE;
    }
  }

  /* input password */
  if (!Password_input(password,"Crypt password") || (Password_length(password) <= 0))
  {
    if ((*cryptPassword) == NULL) Password_delete(password);
    return FALSE;
  }

  /* check password quality */
  if (Password_getQualityLevel(password) < MIN_PASSWORD_QUALITY_LEVEL)
  {
    printWarning("Low password quality!\n");
  }

  (*cryptPassword) = password;

  return TRUE;
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
    #endif /* not NDEBUG */
    return EXITCODE_INIT_FAIL;
  }
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  defaultOptions.barExecutable = argv[0];

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
    #endif /* not NDEBUG */
    return EXITCODE_INVALID_ARGUMENT;
  }

  if (!defaultOptions.noDefaultConfigFlag)
  {
    fileName = String_new();

    /* read default configuration from /CONFIG_DIR/bar.cfg (ignore errors) */
    File_setFileNameCString(fileName,CONFIG_DIR);
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_isFileReadable(fileName))
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
  printInfoFlag = !defaultOptions.quietFlag;
  while (StringList_getFirst(&configFileNameList,fileName) != NULL)
  {
    if (!readConfigFile(fileName,printInfoFlag))
    {
      String_delete(fileName);
      doneAll();
      #ifndef NDEBUG
        Array_debug();
        String_debug();
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
    #endif /* not NDEBUG */
    return EXITCODE_INVALID_ARGUMENT;
  }
  if (versionFlag)
  {
    #ifndef NDEBUG
      printf("BAR version %s (debug)\n",VERSION);
    #else /* NDEBUG */
      printf("BAR version %s\n",VERSION);
    #endif /* not NDEBUG */

    doneAll();
    #ifndef NDEBUG
      Array_debug();
      String_debug();
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
    #endif /* not NDEBUG */
    return EXITCODE_OK;
  }

  /* create session log file */
  File_getTmpFileName(tmpLogFileName,defaultOptions.tmpDirectory);
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
    /* daemon mode -> run server with netwerk */
    if ((serverPort != 0) || (serverTLSPort != 0))
    {
      error = Server_run(serverPort,
                         serverTLSPort,
                         serverCAFileName,
                         serverCertFileName,
                         serverKeyFileName,
                         serverPassword
                        );
    }
    else
    {
      printError("No port number specified!\n");
      error = ERROR_INVALID_ARGUMENT;
    }
  }
  else if (batchFlag)
  {
    /* batch mode -> run server with standard i/o */
    error = Server_batch(STDIN_FILENO,
                         STDOUT_FILENO
                        );
  }
  else
  {
    switch (command)
    {
      case COMMAND_CREATE:
        {
          int    z;
          Errors error;

          /* get archive filename */
          if (argc < 1)
          {
            printError("No archive filename given!\n");
            error = ERROR_INVALID_ARGUMENT;
            break;
          }
          archiveFileName = argv[1];

          /* get include patterns */
          for (z = 2; z < argc; z++)
          {
            error = Pattern_appendList(&includePatternList,argv[z],defaultOptions.patternType);
          }

          /* create archive */
          error = Command_create(archiveFileName,
                                 &includePatternList,
                                 &excludePatternList,
                                 &defaultOptions,
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
                                   &defaultOptions
                                  );
              break;
            case COMMAND_TEST:
              error = Command_test(&fileNameList,
                                   &includePatternList,
                                   &excludePatternList,
                                   &defaultOptions
                                  );
              break;
            case COMMAND_COMPARE:
              error = Command_compare(&fileNameList,
                                      &includePatternList,
                                      &excludePatternList,
                                      &defaultOptions
                                     );
              break;
            case COMMAND_RESTORE:
              error = Command_restore(&fileNameList,
                                      &includePatternList,
                                      &excludePatternList,
                                      &defaultOptions,
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
      default:
        printError("No command given!\n");
        error = ERROR_INVALID_ARGUMENT;
        break;
    }
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
