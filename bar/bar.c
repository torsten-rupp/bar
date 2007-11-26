/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.c,v $
* $Revision: 1.40 $
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

#define DEFAULT_REMOTE_BAR_EXECUTABLE "/home/torsten/bar"

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
LOCAL SSHServerList sshServerList;
LOCAL DeviceList    deviceList;
LOCAL SSHServer     *sshServer = &defaultOptions.defaultSSHServer;
LOCAL Device        *device = &defaultOptions.defaultDevice;
LOCAL bool          daemonFlag;
LOCAL uint          serverPort;
LOCAL bool          serverTLSPort;
LOCAL const char    *serverCAFileName;
LOCAL const char    *serverCertFileName;
LOCAL const char    *serverKeyFileName;
LOCAL Password      *serverPassword;
//LOCAL const char  *logFileName;

LOCAL bool          batchFlag;
LOCAL bool          versionFlag;
LOCAL bool          helpFlag,xhelpFlag,helpInternalFlag;

LOCAL String        outputLine;
LOCAL bool          outputNewLineFlag;

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

LOCAL const CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM         ("create",                   'c',0,0,command,                                              COMMAND_NONE,COMMAND_CREATE,                                       "create new archive"                                               ),
  CMD_OPTION_ENUM         ("list",                     'l',0,0,command,                                              COMMAND_NONE,COMMAND_LIST,                                         "list contents of archive"                                         ),
  CMD_OPTION_ENUM         ("test",                     't',0,0,command,                                              COMMAND_NONE,COMMAND_TEST,                                         "test contents of archive"                                         ),
  CMD_OPTION_ENUM         ("compare",                  'd',0,0,command,                                              COMMAND_NONE,COMMAND_COMPARE,                                      "compare contents of archive with files"                           ),
  CMD_OPTION_ENUM         ("extract",                  'x',0,0,command,                                              COMMAND_NONE,COMMAND_RESTORE,                                      "restore archive"                                                  ),

  CMD_OPTION_SPECIAL      ("config",                   0,  1,0,NULL,                                                 NULL,cmdOptionParseConfigFile,NULL,                                "configuration file","file name"                                   ),

  CMD_OPTION_INTEGER64    ("archive-part-size",        's',1,0,defaultOptions.archivePartSize,                       0,0,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                        "approximated part size"                                           ),
  CMD_OPTION_SPECIAL      ("tmp-directory",            0,  1,0,&defaultOptions.tmpDirectory,                         DEFAULT_TMP_DIRECTORY,cmdOptionParseString,NULL,                   "temporary directory","path"                                       ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",             0,  1,0,defaultOptions.maxTmpSize,                            0,0,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                        "max. size of temporary files"                                     ),
  CMD_OPTION_INTEGER      ("directory-strip",          'p',1,0,defaultOptions.directoryStripCount,                   0,0,LONG_MAX,NULL,                                                 "number of directories to strip on extract"                        ),
  CMD_OPTION_SPECIAL      ("directory",                0,  0,0,&defaultOptions.directory   ,                         NULL,cmdOptionParseString,NULL,                                    "directory to restore files","path"                                ),

  CMD_OPTION_INTEGER      ("max-band-width",           0,  1,0,defaultOptions.maxBandWidth,                          0,0,LONG_MAX,COMMAND_LINE_BITS_UNITS,                              "max. network band width to use"                                   ),

  CMD_OPTION_SELECT       ("pattern-type",             0,  1,0,defaultOptions.patternType,                           PATTERN_TYPE_GLOB,COMMAND_LINE_OPTIONS_PATTERN_TYPES,              "select pattern type"                                              ),

  CMD_OPTION_SPECIAL      ("include",                  'i',0,1,&includePatternList,                                  NULL,cmdOptionParseIncludeExclude,NULL,                            "include pattern","pattern"                                        ),
  CMD_OPTION_SPECIAL      ("exclude",                  '!',0,1,&excludePatternList,                                  NULL,cmdOptionParseIncludeExclude,NULL,                            "exclude pattern","pattern"                                        ),
 
  CMD_OPTION_SELECT       ("compress-algorithm",       'z',0,0,defaultOptions.compressAlgorithm,                     COMPRESS_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHMS,  "select compress algorithm to use"                                 ),
  CMD_OPTION_INTEGER      ("compress-min-size",        0,  1,0,defaultOptions.compressMinFileSize,                   DEFAULT_COMPRESS_MIN_FILE_SIZE,0,LONG_MAX,COMMAND_LINE_BYTES_UNITS,"minimal size of file for compression"                             ),

  CMD_OPTION_SELECT       ("crypt-algorithm",          'y',0,0,defaultOptions.cryptAlgorithm,                        CRYPT_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_CRYPT_ALGORITHMS,        "select crypt algorithm to use"                                    ),
  CMD_OPTION_SPECIAL      ("crypt-password",           0,  0,0,&defaultOptions.cryptPassword,                        NULL,cmdOptionParsePassword,NULL,                                  "crypt password (use with care!)","password"                       ),

  CMD_OPTION_INTEGER      ("ssh-port",                 0,  0,0,defaultOptions.defaultSSHServer.port,                 0,0,65535,NULL,                                                    "ssh port"                                                         ),
  CMD_OPTION_SPECIAL      ("ssh-login-name",           0,  0,0,&defaultOptions.defaultSSHServer.loginName,           NULL,cmdOptionParseString,NULL,                                    "ssh login name","name"                                            ),
  CMD_OPTION_SPECIAL      ("ssh-public-key",           0,  1,0,&defaultOptions.defaultSSHServer.publicKeyFileName,   NULL,cmdOptionParseString,NULL,                                    "ssh public key file name","file name"                             ),
  CMD_OPTION_SPECIAL      ("ssh-privat-key",           0,  1,0,&defaultOptions.defaultSSHServer.privatKeyFileName,   NULL,cmdOptionParseString,NULL,                                    "ssh privat key file name","file name"                             ),
  CMD_OPTION_SPECIAL      ("ssh-password",             0,  0,0,&defaultOptions.defaultSSHServer.password,            NULL,cmdOptionParsePassword,NULL,                                  "ssh password (use with care!)","password"                         ),

  CMD_OPTION_BOOLEAN      ("daemon",                   0,  1,0,daemonFlag,                                           FALSE,                                                             "run in daemon mode"                                               ),
  CMD_OPTION_INTEGER      ("port",                     0,  1,0,serverPort,                                           DEFAULT_SERVER_PORT,0,65535,NULL,                                  "server port"                                                      ),
  CMD_OPTION_INTEGER      ("tls-port",                 0,  1,0,serverTLSPort,                                        DEFAULT_TLS_SERVER_PORT,0,65535,NULL,                              "TLS (SSL) server port"                                            ),
  CMD_OPTION_STRING       ("server-ca-file",           0,  1,0,serverCAFileName,                                     DEFAULT_TLS_SERVER_CA_FILE,                                        "TLS (SSL) server certificate authority file (CA file)","file name"),
  CMD_OPTION_STRING       ("server-cert-file",         0,  1,0,serverCertFileName,                                   DEFAULT_TLS_SERVER_CERTIFICATE_FILE,                               "TLS (SSL) server certificate file","file name"                    ),
  CMD_OPTION_STRING       ("server-key-file",          0,  1,0,serverKeyFileName,                                    DEFAULT_TLS_SERVER_KEY_FILE,                                       "TLS (SSL) server key file","file name"                            ),
  CMD_OPTION_SPECIAL      ("server-password",          0,  1,0,&serverPassword,                                      NULL,cmdOptionParsePassword,NULL,                                  "server password (use with care!)","password"                      ),

  CMD_OPTION_BOOLEAN      ("batch",                    0,  2,0,batchFlag,                                            FALSE,                                                             "run in batch mode"                                                ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",    0,  1,0,&defaultOptions.remoteBARExecutable,                  DEFAULT_REMOTE_BAR_EXECUTABLE,cmdOptionParseString,NULL,           "remote BAR executable","file name"                                ),

  CMD_OPTION_SPECIAL      ("device",                   0,  1,0,&defaultOptions.deviceName,                           DEFAULT_DEVICE_NAME,cmdOptionParseString,NULL,                     "default device","device name"                                     ),
  CMD_OPTION_SPECIAL      ("request-volume-command",   0,  1,0,&defaultOptions.defaultDevice.requestVolumeCommand,   NULL,cmdOptionParseString,NULL,                                    "request new volume command","command"                             ),
  CMD_OPTION_SPECIAL      ("unload-volume-command",    0,  1,0,&defaultOptions.defaultDevice.unloadVolumeCommand,    NULL,cmdOptionParseString,NULL,                                    "unload volume command","command"                                  ),
  CMD_OPTION_SPECIAL      ("load-volume-command",      0,  1,0,&defaultOptions.defaultDevice.loadVolumeCommand,      NULL,cmdOptionParseString,NULL,                                    "load volume command","command"                                    ),
  CMD_OPTION_INTEGER64    ("volume-size",              0,  1,0,defaultOptions.defaultDevice.volumeSize,              0LL,0LL,LONG_LONG_MAX,COMMAND_LINE_BYTES_UNITS,                    "volume size"                                                      ),
  CMD_OPTION_SPECIAL      ("device-image-pre-command", 0,  1,0,&defaultOptions.defaultDevice.imagePreProcessCommand, NULL,cmdOptionParseString,NULL,                                    "make image pre-process command","command"                         ),
  CMD_OPTION_SPECIAL      ("device-image-post-command",0,  1,0,&defaultOptions.defaultDevice.imagePostProcessCommand,NULL,cmdOptionParseString,NULL,                                    "make image post-process command","command"                        ),
  CMD_OPTION_SPECIAL      ("device-image-command",     0,  1,0,&defaultOptions.defaultDevice.imageCommand,           NULL,cmdOptionParseString,NULL,                                    "make image command","command"                                     ),
  CMD_OPTION_SPECIAL      ("device-ecc-pre-command",   0,  1,0,&defaultOptions.defaultDevice.eccPreProcessCommand,   NULL,cmdOptionParseString,NULL,                                    "make error-correction codes pre-process command","command"        ),
  CMD_OPTION_SPECIAL      ("device-ecc-post-command",  0,  1,0,&defaultOptions.defaultDevice.eccPostProcessCommand,  NULL,cmdOptionParseString,NULL,                                    "make error-correction codes post-process command","command"       ),
  CMD_OPTION_SPECIAL      ("device-ecc-command",       0,  1,0,&defaultOptions.defaultDevice.eccCommand,             NULL,cmdOptionParseString,NULL,                                    "make error-correction codes command","command"                    ),
  CMD_OPTION_SPECIAL      ("device-write-pre-command", 0,  1,0,&defaultOptions.defaultDevice.writePreProcessCommand, NULL,cmdOptionParseString,NULL,                                    "write device pre-process command","command"                       ),
  CMD_OPTION_SPECIAL      ("device-write-post-command",0,  1,0,&defaultOptions.defaultDevice.writePostProcessCommand,NULL,cmdOptionParseString,NULL,                                    "write device post-process command","command"                      ),
  CMD_OPTION_SPECIAL      ("device-write-command",     0,  1,0,&defaultOptions.defaultDevice.writeCommand,           NULL,cmdOptionParseString,NULL,                                    "write device command","command"                                   ),

  CMD_OPTION_BOOLEAN      ("ecc",                      0,  1,0,defaultOptions.errorCorrectionCodesFlag,              FALSE,                                                             "add error-correction codes with 'dvdisaster' tool"                ),

//  CMD_OPTION_BOOLEAN      ("incremental",              0,  0,0,defaultOptions.incrementalFlag,                     FALSE,                                                             "overwrite existing files"                                         ),
  CMD_OPTION_BOOLEAN      ("skip-unreadable",          0,  0,0,defaultOptions.skipUnreadableFlag,                    TRUE,                                                              "skip unreadable files"                                            ),
  CMD_OPTION_BOOLEAN      ("overwrite-archive-files",  0,  0,0,defaultOptions.overwriteArchiveFilesFlag,             FALSE,                                                             "overwrite existing archive files"                                 ),
  CMD_OPTION_BOOLEAN      ("overwrite-files",          0,  0,0,defaultOptions.overwriteFilesFlag,                    FALSE,                                                             "overwrite existing files"                                         ),
  CMD_OPTION_BOOLEAN      ("no-default-config",        0,  1,0,defaultOptions.noDefaultConfigFlag,                   FALSE,                                                             "do not read personal config file ~/.bar/" DEFAULT_CONFIG_FILE_NAME),
  CMD_OPTION_BOOLEAN      ("quiet",                    0,  1,0,defaultOptions.quietFlag,                             FALSE,                                                             "surpress any output"                                              ),
  CMD_OPTION_INTEGER_RANGE("verbose",                  'v',1,0,defaultOptions.verboseLevel,                          1,0,3,NULL,                                                        "verbosity level"                                                  ),

  CMD_OPTION_BOOLEAN      ("version",                  0  ,0,0,versionFlag,                                          FALSE,                                                             "output version"                                                   ),
  CMD_OPTION_BOOLEAN      ("help",                     'h',0,0,helpFlag,                                             FALSE,                                                             "output this help"                                                 ),
  CMD_OPTION_BOOLEAN      ("xhelp",                    'h',0,0,xhelpFlag,                                            FALSE,                                                             "output help to extended options"                                  ),
  CMD_OPTION_BOOLEAN      ("help-internal",            'h',1,0,helpInternalFlag,                                     FALSE,                                                             "output help to internal options"                                  ),
};

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

LOCAL const ConfigValue CONFIG_VALUES[] =
{
  CONFIG_VALUE_SPECIAL  ("config",                   NULL,-1,                                                 configValueParseConfigFile,NULL),

  CONFIG_VALUE_INTEGER64("archive-part-size",        defaultOptions.archivePartSize,-1,                       0,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL  ("tmp-directory",            &defaultOptions.tmpDirectory,-1,                         configValueParseString,NULL),
  CONFIG_VALUE_INTEGER64("max-tmp-size",             defaultOptions.maxTmpSize,-1,                            0,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_INTEGER  ("directory-strip",          defaultOptions.directoryStripCount,-1,                   0,LONG_MAX,NULL),
  CONFIG_VALUE_SPECIAL  ("directory",                &defaultOptions.directory,-1,                            configValueParseString,NULL),

  CONFIG_VALUE_INTEGER  ("max-band-width",           defaultOptions.maxBandWidth,-1,                          0,LONG_MAX,CONFIG_VALUE_BITS_UNITS),

  CONFIG_VALUE_SELECT   ("pattern-type",             defaultOptions.patternType,-1,                           CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_VALUE_SPECIAL  ("include",                  &includePatternList,-1,                                  configValueParseIncludeExclude,NULL),
  CONFIG_VALUE_SPECIAL  ("exclude",                  &excludePatternList,-1,                                  configValueParseIncludeExclude,NULL),
 
  CONFIG_VALUE_SELECT   ("compress-algorithm",       defaultOptions.compressAlgorithm,-1,                     CONFIG_VALUE_COMPRESS_ALGORITHMS),
  CONFIG_VALUE_INTEGER  ("compress-min-size",        defaultOptions.compressMinFileSize,-1,                   0,LONG_MAX,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_VALUE_SELECT   ("crypt-algorithm",          defaultOptions.cryptAlgorithm,-1,                        CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_VALUE_SPECIAL  ("crypt-password",           &defaultOptions.cryptPassword,-1,                        configValueParsePassword,NULL),

  CONFIG_VALUE_INTEGER  ("ssh-port",                 sshServer,offsetof(SSHServer,port),                      0,65535,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-login-name",           &sshServer,offsetof(SSHServer,loginName),                configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-public-key",           &sshServer,offsetof(SSHServer,publicKeyFileName),        configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-privat-key",           &sshServer,offsetof(SSHServer,privatKeyFileName),        configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("ssh-password",             &sshServer,offsetof(SSHServer,password),                 configValueParsePassword,NULL),

  CONFIG_VALUE_INTEGER  ("port",                     serverPort,-1,                                           0,65535,NULL),
  CONFIG_VALUE_INTEGER  ("tls-port",                 serverTLSPort,-1,                                        0,65535,NULL),
  CONFIG_VALUE_STRING   ("server-ca-file",           serverCAFileName,-1                                      ),
  CONFIG_VALUE_STRING   ("server-cert-file",         serverCertFileName,-1                                    ),
  CONFIG_VALUE_STRING   ("server-key-file",          serverKeyFileName,-1                                     ),
  CONFIG_VALUE_SPECIAL  ("server-password",          &serverPassword,-1,                                      configValueParsePassword,NULL),

  CONFIG_VALUE_SPECIAL  ("remote-bar-executable",    &defaultOptions.remoteBARExecutable,-1,                  configValueParseString,NULL),

  CONFIG_VALUE_SPECIAL  ("device",                   &defaultOptions.deviceName,-1,                           configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("request-volume-command",   &defaultOptions.defaultDevice.requestVolumeCommand,-1,   configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("unload-volume-command",    &defaultOptions.defaultDevice.unloadVolumeCommand,-1,    configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("load-volume-command",      &defaultOptions.defaultDevice.loadVolumeCommand,-1,      configValueParseString,NULL),
  CONFIG_VALUE_INTEGER64("volume-size",              defaultOptions.defaultDevice.volumeSize,-1,              0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_VALUE_SPECIAL  ("device-image-pre-command", &defaultOptions.defaultDevice.imagePreProcessCommand,-1, configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-image-post-command",&defaultOptions.defaultDevice.imagePostProcessCommand,-1,configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-image-command",     &defaultOptions.defaultDevice.imageCommand,-1,           configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-ecc-pre-command",   &defaultOptions.defaultDevice.eccPreProcessCommand,-1,   configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-ecc-post-command",  &defaultOptions.defaultDevice.eccPostProcessCommand,-1,  configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-ecc-command",       &defaultOptions.defaultDevice.eccCommand,-1,             configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-write-pre-command", &defaultOptions.defaultDevice.writePreProcessCommand,-1, configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-write-post-command",&defaultOptions.defaultDevice.writePostProcessCommand,-1,configValueParseString,NULL),
  CONFIG_VALUE_SPECIAL  ("device-write-command",     &defaultOptions.defaultDevice.writeCommand,-1,           configValueParseString,NULL),

  CONFIG_VALUE_BOOLEAN  ("ecc",                      defaultOptions.errorCorrectionCodesFlag,-1               ),

  CONFIG_VALUE_BOOLEAN  ("skip-unreadable",          defaultOptions.skipUnreadableFlag,-1                     ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-archive-files",  defaultOptions.overwriteArchiveFilesFlag,-1              ),
  CONFIG_VALUE_BOOLEAN  ("overwrite-files",          defaultOptions.overwriteFilesFlag,-1                     ),
  CONFIG_VALUE_BOOLEAN  ("no-default-config",        defaultOptions.noDefaultConfigFlag,-1                    ),
  CONFIG_VALUE_BOOLEAN  ("quiet",                    defaultOptions.quietFlag,-1                              ),
  CONFIG_VALUE_INTEGER  ("verbose",                  defaultOptions.verboseLevel,-1,                          0,3,NULL),
};

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : outputConsole
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

LOCAL void outputConsole(FILE *file, bool saveRestoreFlag, const String string)
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

LOCAL bool readConfigFile(String fileName, bool printErrorFlag)
{
  Errors      error;
  FileHandle  fileHandle;
  bool        failFlag;
  uint        lineNb;
  String      line;
  String      name,value;

  assert(fileName != NULL);

  /* open file */
  error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    if (printErrorFlag)
    {
      printError("Cannot open file '%s' (error: %s)!\n",
                 String_cString(fileName),
                 getErrorText(error)
                );
    }
    return FALSE;
  }

  /* parse file */
  info(2,"Reading config file '%s'...",String_cString(fileName));
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
      info(2,"FAIL\n");
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
      sshServerNode->name                        = String_copy(name);
      sshServerNode->sshServer.port              = 0;
      sshServerNode->sshServer.loginName         = NULL;
      sshServerNode->sshServer.publicKeyFileName = NULL;
      sshServerNode->sshServer.privatKeyFileName = NULL;
      sshServerNode->sshServer.password          = NULL;

      List_append(defaultOptions.sshServerList,sshServerNode);

      sshServer = &sshServerNode->sshServer;
    }
    else if (String_parse(line,"[device %S]",NULL,name))
    {
      DeviceNode *deviceNode;

      deviceNode = LIST_NEW_NODE(DeviceNode);
      if (deviceNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      deviceNode->name                           = String_copy(name);
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

      List_append(defaultOptions.deviceList,deviceNode);

      device = &deviceNode->device;
    }
    else if (String_parse(line,"[global]",NULL))
    {
      sshServer = &defaultOptions.defaultSSHServer;
      device    = &defaultOptions.device;
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
        info(2,"FAIL\n");
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
      info(2,"FAIL\n");
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
    info(2,"ok\n");
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
  String fileName;
  bool   result;

  assert(value != NULL);

  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  fileName = String_newCString(value);
  result = readConfigFile(fileName,TRUE);
  String_delete(fileName);

  return result;
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
  String fileName;
  bool   result;

  assert(value != NULL);

  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(userData);

  fileName = String_newCString(value);
  result = readConfigFile(fileName,TRUE);
  String_delete(fileName);

  return result;
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

/***********************************************************************\
* Name   : init
* Purpose: initialize
* Input  : -
* Output : -
* Return : TRUE if init ok, FALSE on error
* Notes  : -
\***********************************************************************/

LOCAL bool initAll(void)
{
  Errors error;

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

  return TRUE;
}

/***********************************************************************\
* Name   : done
* Purpose: deinitialize
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneAll(void)
{
  Server_doneAll();
  Network_doneAll();
  Storage_doneAll();
  Archive_doneAll();
  Pattern_doneAll();
  Crypt_doneAll();
  Password_doneAll();
}

LOCAL void initOptions(Options *options)
{
  assert(options != NULL);

  memset(options,0,sizeof(Options));
  options->sshServerList = &sshServerList;
  options->deviceList    = &deviceList;
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

/*---------------------------------------------------------------------*/

void vinfo(uint verboseLevel, const char *format, va_list arguments)
{
  String line;

  assert(format != NULL);

  if (!defaultOptions.quietFlag && (defaultOptions.verboseLevel >= verboseLevel))
  {
    line = String_new();

    /* format line */
    String_vformat(line,format,arguments);

    /* output */
    outputConsole(stdout,FALSE,line);

    String_delete(line);
  }
}

void info(uint verboseLevel, const char *format, ...)
{
  String  line;
  va_list arguments;

  assert(format != NULL);

  if (!defaultOptions.quietFlag && (defaultOptions.verboseLevel >= verboseLevel))
  {
    line = String_new();

    /* format line */
    va_start(arguments,format);
    String_vformat(line,format,arguments);
    va_end(arguments);

    /* output */
    outputConsole(stdout,FALSE,line);

    String_delete(line);
  }
}

void printWarning(const char *text, ...)
{
  String  line;
  va_list arguments;

  assert(text != NULL);

  line = String_new();

  /* format line */
  va_start(arguments,text);
  String_appendCString(line,"Warning: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  /* output */
  outputConsole(stdout,TRUE,line);

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
  String_appendCString(line,"ERROR: ");
  String_vformat(line,text,arguments);
  va_end(arguments);

  /* output */
  outputConsole(stderr,TRUE,line);

  String_delete(line);
}

void copyOptions(const Options *sourceOptions, Options *destinationOptions)
{
  assert(sourceOptions != NULL);
  assert(destinationOptions != NULL);

  memcpy(destinationOptions,sourceOptions,sizeof(Options));
  destinationOptions->tmpDirectory                          = String_copy(sourceOptions->tmpDirectory);         
  destinationOptions->directory                             = String_copy(sourceOptions->directory);            
  destinationOptions->cryptPassword                         = Password_copy(sourceOptions->cryptPassword);      
  destinationOptions->sshServer.loginName                   = String_copy(sourceOptions->sshServer.loginName); 
  destinationOptions->sshServer.publicKeyFileName           = String_copy(sourceOptions->sshServer.publicKeyFileName); 
  destinationOptions->sshServer.privatKeyFileName           = String_copy(sourceOptions->sshServer.privatKeyFileName); 
  destinationOptions->sshServer.password                    = Password_copy(sourceOptions->sshServer.password);        
  destinationOptions->defaultSSHServer.loginName            = String_copy(sourceOptions->defaultSSHServer.loginName); 
  destinationOptions->defaultSSHServer.publicKeyFileName    = String_copy(sourceOptions->defaultSSHServer.publicKeyFileName); 
  destinationOptions->defaultSSHServer.privatKeyFileName    = String_copy(sourceOptions->defaultSSHServer.privatKeyFileName); 
  destinationOptions->defaultSSHServer.password             = Password_copy(sourceOptions->defaultSSHServer.password);        
  destinationOptions->remoteBARExecutable                   = String_copy(sourceOptions->remoteBARExecutable);  
  destinationOptions->deviceName                            = String_copy(sourceOptions->deviceName);        
  destinationOptions->defaultDevice.requestVolumeCommand    = String_copy(sourceOptions->defaultDevice.requestVolumeCommand);
  destinationOptions->defaultDevice.unloadVolumeCommand     = String_copy(sourceOptions->defaultDevice.unloadVolumeCommand);
  destinationOptions->defaultDevice.loadVolumeCommand       = String_copy(sourceOptions->defaultDevice.loadVolumeCommand);
  destinationOptions->defaultDevice.imagePreProcessCommand  = String_copy(sourceOptions->defaultDevice.imagePreProcessCommand);
  destinationOptions->defaultDevice.imagePostProcessCommand = String_copy(sourceOptions->defaultDevice.imagePostProcessCommand);
  destinationOptions->defaultDevice.imageCommand            = String_copy(sourceOptions->defaultDevice.imageCommand);
  destinationOptions->defaultDevice.eccPreProcessCommand    = String_copy(sourceOptions->defaultDevice.eccPreProcessCommand);
  destinationOptions->defaultDevice.eccPostProcessCommand   = String_copy(sourceOptions->defaultDevice.eccPostProcessCommand);
  destinationOptions->defaultDevice.eccCommand              = String_copy(sourceOptions->defaultDevice.eccCommand);
  destinationOptions->defaultDevice.writePreProcessCommand  = String_copy(sourceOptions->defaultDevice.writePreProcessCommand);
  destinationOptions->defaultDevice.writePostProcessCommand = String_copy(sourceOptions->defaultDevice.writePostProcessCommand);
  destinationOptions->defaultDevice.writeCommand            = String_copy(sourceOptions->defaultDevice.writeCommand);
}

void freeOptions(Options *options)
{
  assert(options != NULL);

  String_delete(options->tmpDirectory);
  String_delete(options->directory);
  Password_delete(options->cryptPassword);
  String_delete(options->sshServer.loginName);
  String_delete(options->sshServer.privatKeyFileName);
  String_delete(options->sshServer.publicKeyFileName);
  Password_delete(options->sshServer.password);
  String_delete(options->defaultSSHServer.loginName);
  String_delete(options->defaultSSHServer.privatKeyFileName);
  String_delete(options->defaultSSHServer.publicKeyFileName);
  Password_delete(options->defaultSSHServer.password);
  String_delete(options->remoteBARExecutable);
  String_delete(options->deviceName);
  String_delete(options->defaultDevice.loadVolumeCommand);
  String_delete(options->defaultDevice.unloadVolumeCommand);
  String_delete(options->defaultDevice.requestVolumeCommand);
  String_delete(options->defaultDevice.imagePreProcessCommand);
  String_delete(options->defaultDevice.imagePostProcessCommand);
  String_delete(options->defaultDevice.imageCommand);
  String_delete(options->defaultDevice.eccPreProcessCommand);
  String_delete(options->defaultDevice.eccPostProcessCommand);
  String_delete(options->defaultDevice.eccCommand);
  String_delete(options->defaultDevice.writePreProcessCommand);
  String_delete(options->defaultDevice.writePostProcessCommand);
  String_delete(options->defaultDevice.writeCommand);
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
  sshServer->port              = (options->sshServer.port              != 0)?options->sshServer.port             :((sshServerNode != NULL)?sshServerNode->sshServer.port             :options->defaultSSHServer.port             );
  sshServer->loginName         = (options->sshServer.loginName         != 0)?options->sshServer.loginName        :((sshServerNode != NULL)?sshServerNode->sshServer.loginName        :options->defaultSSHServer.loginName        );
  sshServer->publicKeyFileName = (options->sshServer.publicKeyFileName != 0)?options->sshServer.publicKeyFileName:((sshServerNode != NULL)?sshServerNode->sshServer.publicKeyFileName:options->defaultSSHServer.publicKeyFileName);
  sshServer->privatKeyFileName = (options->sshServer.privatKeyFileName != 0)?options->sshServer.privatKeyFileName:((sshServerNode != NULL)?sshServerNode->sshServer.privatKeyFileName:options->defaultSSHServer.privatKeyFileName);
  sshServer->password          = (options->sshServer.password          != 0)?options->sshServer.password         :((sshServerNode != NULL)?sshServerNode->sshServer.password         :options->defaultSSHServer.password         );
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
  device->requestVolumeCommand    = (options->device.requestVolumeCommand    != 0)?options->device.requestVolumeCommand   :((deviceNode != NULL)?deviceNode->device.requestVolumeCommand   :options->defaultDevice.requestVolumeCommand   );
  device->unloadVolumeCommand     = (options->device.unloadVolumeCommand     != 0)?options->device.unloadVolumeCommand    :((deviceNode != NULL)?deviceNode->device.unloadVolumeCommand    :options->defaultDevice.unloadVolumeCommand    );
  device->loadVolumeCommand       = (options->device.loadVolumeCommand       != 0)?options->device.loadVolumeCommand      :((deviceNode != NULL)?deviceNode->device.loadVolumeCommand      :options->defaultDevice.loadVolumeCommand      );
  device->volumeSize              = (options->device.volumeSize              != 0)?options->device.volumeSize             :((deviceNode != NULL)?deviceNode->device.volumeSize             :options->defaultDevice.volumeSize             );
  device->imagePreProcessCommand  = (options->device.imagePreProcessCommand  != 0)?options->device.imagePreProcessCommand :((deviceNode != NULL)?deviceNode->device.imagePreProcessCommand :options->defaultDevice.imagePreProcessCommand );
  device->imagePostProcessCommand = (options->device.imagePostProcessCommand != 0)?options->device.imagePostProcessCommand:((deviceNode != NULL)?deviceNode->device.imagePostProcessCommand:options->defaultDevice.imagePostProcessCommand);
  device->imageCommand            = (options->device.imageCommand            != 0)?options->device.imageCommand           :((deviceNode != NULL)?deviceNode->device.imageCommand           :options->defaultDevice.imageCommand           );
  device->eccPreProcessCommand    = (options->device.eccPreProcessCommand    != 0)?options->device.eccPreProcessCommand   :((deviceNode != NULL)?deviceNode->device.eccPreProcessCommand   :options->defaultDevice.eccPreProcessCommand   );
  device->eccPostProcessCommand   = (options->device.eccPostProcessCommand   != 0)?options->device.eccPostProcessCommand  :((deviceNode != NULL)?deviceNode->device.eccPostProcessCommand  :options->defaultDevice.eccPostProcessCommand  );
  device->eccCommand              = (options->device.eccCommand              != 0)?options->device.eccCommand             :((deviceNode != NULL)?deviceNode->device.eccCommand             :options->defaultDevice.eccCommand             );
  device->writePreProcessCommand  = (options->device.writePreProcessCommand  != 0)?options->device.writePreProcessCommand :((deviceNode != NULL)?deviceNode->device.writePreProcessCommand :options->defaultDevice.writePreProcessCommand );
  device->writePostProcessCommand = (options->device.writePostProcessCommand != 0)?options->device.writePostProcessCommand:((deviceNode != NULL)?deviceNode->device.writePostProcessCommand:options->defaultDevice.writePostProcessCommand);
  device->writeCommand            = (options->device.writeCommand            != 0)?options->device.writeCommand           :((deviceNode != NULL)?deviceNode->device.writeCommand           :options->defaultDevice.writeCommand           );
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  String fileName;
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

  /* initialise variables */
  outputLine = String_new();
  outputNewLineFlag = TRUE;
  initOptions(&defaultOptions);
  serverPassword = Password_new();
  Pattern_initList(&includePatternList);
  Pattern_initList(&excludePatternList);
  List_init(&sshServerList);
  List_init(&deviceList);
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));

  /* parse command line: pre-options */
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,
                       stderr,NULL
                      )
     )
  {
    List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
    List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
    Pattern_doneList(&excludePatternList);
    Pattern_doneList(&includePatternList);
    Password_delete(serverPassword);
    freeOptions(&defaultOptions);
    String_delete(outputLine);
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
      if (!readConfigFile(fileName,TRUE))
      {
        String_delete(fileName);
        List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
        List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
        Pattern_doneList(&excludePatternList);
        Pattern_doneList(&includePatternList);
        Password_delete(serverPassword);
        freeOptions(&defaultOptions);
        String_delete(outputLine);
        doneAll();
        #ifndef NDEBUG
          Array_debug();
          String_debug();
        #endif /* not NDEBUG */
        return EXITCODE_CONFIG_ERROR;
      }
    }

    /* read default configuration from $HOME/.bar/bar.cfg (if exists) */
    File_setFileNameCString(fileName,getenv("HOME"));
    File_appendFileNameCString(fileName,".bar");
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName))
    {
      if (!readConfigFile(fileName,TRUE))
      {
        String_delete(fileName);
        List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
        List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
        Pattern_doneList(&excludePatternList);
        Pattern_doneList(&includePatternList);
        Password_delete(serverPassword);
        freeOptions(&defaultOptions);
        String_delete(outputLine);
        doneAll();
        #ifndef NDEBUG
          Array_debug();
          String_debug();
        #endif /* not NDEBUG */
        return EXITCODE_CONFIG_ERROR;
      }
    }

    String_delete(fileName);
  }

  /* parse command line: all */
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       CMD_PRIORITY_ANY,
                       stderr,NULL
                      )
     )
  {
    List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
    List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
    Pattern_doneList(&excludePatternList);
    Pattern_doneList(&includePatternList);
    Password_delete(serverPassword);
    freeOptions(&defaultOptions);
    String_delete(outputLine);
    doneAll();
    #ifndef NDEBUG
      Array_debug();
      String_debug();
    #endif /* not NDEBUG */
    return EXITCODE_INVALID_ARGUMENT;
  }
  if (versionFlag)
  {
    printf("BAR version %s\n",VERSION);

    List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
    List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
    Pattern_doneList(&excludePatternList);
    Pattern_doneList(&includePatternList);
    Password_delete(serverPassword);
    freeOptions(&defaultOptions);
    String_delete(outputLine);
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

    List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
    List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
    Pattern_doneList(&excludePatternList);
    Pattern_doneList(&includePatternList);
    Password_delete(serverPassword);
    freeOptions(&defaultOptions);
    String_delete(outputLine);
    doneAll();
    #ifndef NDEBUG
      Array_debug();
      String_debug();
    #endif /* not NDEBUG */
    return EXITCODE_OK;
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
        }
        break;
      default:
        printError("No command given!\n");
        error = ERROR_INVALID_ARGUMENT;
        break;
    }
  }

  /* free resources */
  CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  List_done(&deviceList,(ListNodeFreeFunction)freeDeviceNode,NULL);
  List_done(&sshServerList,(ListNodeFreeFunction)freeSSHServerNode,NULL);
  Pattern_doneList(&excludePatternList);
  Pattern_doneList(&includePatternList);
  Password_delete(serverPassword);
  freeOptions(&defaultOptions);
  String_delete(outputLine);
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
