/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.c,v $
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
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "cmdoptions.h"
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

LOCAL Commands    command;
LOCAL const char  *archiveFileName;
LOCAL PatternList includePatternList;
LOCAL PatternList excludePatternList;
LOCAL bool        daemonFlag;
LOCAL uint        serverPort;
LOCAL bool        serverTLSPort;
LOCAL const char  *serverCAFileName;
LOCAL const char  *serverCertFileName;
LOCAL const char  *serverKeyFileName;
LOCAL Password    *serverPassword;
//LOCAL const char  *logFileName;

LOCAL bool        batchFlag;
LOCAL bool        versionFlag;
LOCAL bool        helpFlag,xhelpFlag,helpInternalFlag;

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

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_PATTERN_TYPE[] =
{
  {"glob",    PATTERN_TYPE_GLOB,          "glob patterns: * and ?"},
  {"regex",   PATTERN_TYPE_REGEX,         "regular expression pattern matching"},
  {"extended",PATTERN_TYPE_EXTENDED_REGEX,"extended regular expression pattern matching"},
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHM[] =
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

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_CRYPT_ALGORITHM[] =
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

LOCAL bool cmdParseString(void *variable, const char *name, const char *value, const void *defaultValue, void *userData);
LOCAL bool cmdParseConfigFile(void *variable, const char *name, const char *value, const void *defaultValue, void *userData);
LOCAL bool cmdParseIncludeExclude(void *variable, const char *name, const char *value, const void *defaultValue, void *userData);
LOCAL bool cmdParsePassword(void *variable, const char *name, const char *value, const void *defaultValue, void *userData);

LOCAL const CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM         ("create",                   'c',0,0,command,                                 COMMAND_NONE,COMMAND_CREATE,                                       "create new archive"                                               ),
  CMD_OPTION_ENUM         ("list",                     'l',0,0,command,                                 COMMAND_NONE,COMMAND_LIST,                                         "list contents of archive"                                         ),
  CMD_OPTION_ENUM         ("test",                     't',0,0,command,                                 COMMAND_NONE,COMMAND_TEST,                                         "test contents of archive"                                         ),
  CMD_OPTION_ENUM         ("compare",                  'd',0,0,command,                                 COMMAND_NONE,COMMAND_COMPARE,                                      "compare contents of archive with files"                           ),
  CMD_OPTION_ENUM         ("extract",                  'x',0,0,command,                                 COMMAND_NONE,COMMAND_RESTORE,                                      "restore archive"                                                  ),

  CMD_OPTION_SPECIAL      ("config",                   0,  1,0,NULL,                                    NULL,cmdParseConfigFile,NULL,                                      "configuration file","file name"                                   ),

  CMD_OPTION_INTEGER64    ("archive-part-size",        's',1,0,defaultOptions.archivePartSize,          0,0,LONG_MAX,COMMAND_LINE_BYTES_UNITS,                             "approximated part size"                                           ),
  CMD_OPTION_SPECIAL      ("tmp-directory",            0,  1,0,&defaultOptions.tmpDirectory,            DEFAULT_TMP_DIRECTORY,cmdParseString,NULL,                         "temporary directory","path"                                       ),
  CMD_OPTION_INTEGER64    ("max-tmp-size",             0,  1,0,defaultOptions.maxTmpSize,               0,0,LONG_MAX,COMMAND_LINE_BYTES_UNITS,                             "max. size of temporary files"                                     ),
  CMD_OPTION_INTEGER      ("directory-strip",          'p',1,0,defaultOptions.directoryStripCount,      0,0,LONG_MAX,NULL,                                                 "number of directories to strip on extract"                        ),
  CMD_OPTION_SPECIAL      ("directory",                0,  0,0,&defaultOptions.directory   ,            NULL,cmdParseString,NULL,                                          "directory to restore files","path"                                ),

  CMD_OPTION_INTEGER      ("max-band-width",           0,  1,0,defaultOptions.maxBandWidth,             0,0,LONG_MAX,COMMAND_LINE_BITS_UNITS,                              "max. network band width to use"                                   ),

  CMD_OPTION_SELECT       ("pattern-type",             0,  1,0,defaultOptions.patternType,              PATTERN_TYPE_GLOB,COMMAND_LINE_OPTIONS_PATTERN_TYPE,               "select pattern type"                                              ),

  CMD_OPTION_SPECIAL      ("include",                  'i',0,1,&includePatternList,                     NULL,cmdParseIncludeExclude,NULL,                                  "include pattern","pattern"                                        ),
  CMD_OPTION_SPECIAL      ("exclude",                  '!',0,1,&excludePatternList,                     NULL,cmdParseIncludeExclude,NULL,                                  "exclude pattern","pattern"                                        ),
 
  CMD_OPTION_SELECT       ("compress-algorithm",       'z',0,0,defaultOptions.compressAlgorithm,        COMPRESS_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHM,   "select compress algorithm to use"                                 ),
  CMD_OPTION_INTEGER      ("compress-min-size",        0,  1,0,defaultOptions.compressMinFileSize,      DEFAULT_COMPRESS_MIN_FILE_SIZE,0,LONG_MAX,COMMAND_LINE_BYTES_UNITS,"minimal size of file for compression"                             ),

  CMD_OPTION_SELECT       ("crypt-algorithm",          'y',0,0,defaultOptions.cryptAlgorithm,           CRYPT_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_CRYPT_ALGORITHM,         "select crypt algorithm to use"                                    ),
  CMD_OPTION_SPECIAL      ("crypt-password",           0,  0,0,&defaultOptions.cryptPassword,           NULL,cmdParsePassword,NULL,                                        "crypt password (use with care!)","password"                       ),

  CMD_OPTION_INTEGER      ("ssh-port",                 0,  0,0,defaultOptions.sshPort,                  0,0,65535,NULL,                                                    "ssh port"                                                         ),
  CMD_OPTION_SPECIAL      ("ssh-public-key",           0,  1,0,&defaultOptions.sshPublicKeyFileName,    NULL,cmdParseString,NULL,                                          "ssh public key file name","file name"                             ),
  CMD_OPTION_SPECIAL      ("ssh-privat-key",           0,  1,0,&defaultOptions.sshPrivatKeyFileName,    NULL,cmdParseString,NULL,                                          "ssh privat key file name","file name"                             ),
  CMD_OPTION_SPECIAL      ("ssh-password",             0,  0,0,&defaultOptions.sshPassword,             NULL,cmdParsePassword,NULL,                                        "ssh password (use with care!)","password"                         ),

  CMD_OPTION_BOOLEAN      ("daemon",                   0,  1,0,daemonFlag,                              FALSE,                                                             "run in daemon mode"                                               ),
  CMD_OPTION_INTEGER      ("port",                     0,  1,0,serverPort,                              DEFAULT_SERVER_PORT,0,65535,NULL,                                  "server port"                                                      ),
  CMD_OPTION_INTEGER      ("tls-port",                 0,  1,0,serverTLSPort,                           DEFAULT_TLS_SERVER_PORT,0,65535,NULL,                              "TLS (SSL) server port"                                            ),
  CMD_OPTION_STRING       ("server-ca-file",           0,  1,0,serverCAFileName,                        DEFAULT_TLS_SERVER_CA_FILE,                                        "TLS (SSL) server certificate authority file (CA file)","file name"),
  CMD_OPTION_STRING       ("server-cert-file",         0,  1,0,serverCertFileName,                      DEFAULT_TLS_SERVER_CERTIFICATE_FILE,                               "TLS (SSL) server certificate file","file name"                    ),
  CMD_OPTION_STRING       ("server-key-file",          0,  1,0,serverKeyFileName,                       DEFAULT_TLS_SERVER_KEY_FILE,                                       "TLS (SSL) server key file","file name"                            ),
  CMD_OPTION_SPECIAL      ("server-password",          0,  1,0,&serverPassword,                         NULL,cmdParsePassword,NULL,                                        "server password (use with care!)","password"                      ),

  CMD_OPTION_BOOLEAN      ("batch",                    0,  2,0,batchFlag,                               FALSE,                                                             "run in batch mode"                                                ),
  CMD_OPTION_SPECIAL      ("remote-bar-executable",    0,  1,0,&defaultOptions.remoteBARExecutable,     DEFAULT_REMOTE_BAR_EXECUTABLE,cmdParseString,NULL,                 "remote BAR executable","file name"                                ),

//  CMD_OPTION_BOOLEAN      ("incremental",              0,  0,0,defaultOptions.incrementalFlag,        FALSE,                                                             "overwrite existing files"                                         ),
  CMD_OPTION_BOOLEAN      ("skip-unreadable",          0,  0,0,defaultOptions.skipUnreadableFlag,       TRUE,                                                              "skip unreadable files"                                            ),
  CMD_OPTION_BOOLEAN      ("overwrite-archives-files", 0,  0,0,defaultOptions.overwriteArchiveFilesFlag,FALSE,                                                             "overwrite existing archive files"                                 ),
  CMD_OPTION_BOOLEAN      ("overwrite-files",          0,  0,0,defaultOptions.overwriteFilesFlag,       FALSE,                                                             "overwrite existing files"                                         ),
  CMD_OPTION_BOOLEAN      ("no-default-config",        0,  1,0,defaultOptions.noDefaultConfigFlag,      FALSE,                                                             "do not read personal config file ~/bar/" DEFAULT_CONFIG_FILE_NAME ),
  CMD_OPTION_BOOLEAN      ("quiet",                    0,  1,0,defaultOptions.quietFlag,                FALSE,                                                             "surpress any output"                                              ),
  CMD_OPTION_INTEGER_RANGE("verbose",                  'v',1,0,defaultOptions.verboseLevel,             1,0,3,NULL,                                                        "verbosity level"                                                  ),

  CMD_OPTION_BOOLEAN      ("version",                  0  ,0,0,versionFlag,                             FALSE,                                                             "output version"                                                   ),
  CMD_OPTION_BOOLEAN      ("help",                     'h',0,0,helpFlag,                                FALSE,                                                             "output this help"                                                 ),
  CMD_OPTION_BOOLEAN      ("xhelp",                    'h',0,0,xhelpFlag,                               FALSE,                                                             "output help to extended options"                                  ),
  CMD_OPTION_BOOLEAN      ("help-internal",            'h',1,0,helpInternalFlag,                        FALSE,                                                             "output help to internal options"                                  ),
};

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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
  Errors                  error;
  FileHandle              fileHandle;
  bool                    failFlag;
  uint                    lineNb;
  String                  line;
  String                  name,value;
  const CommandLineOption *commandLineOption;

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
  failFlag = FALSE;
  lineNb   = 0;
  line     = String_new();
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
    if (String_parse(line,"%S=%S",NULL,name,value))
    {
//fprintf(stderr,"%s,%d: %s %s\n",__FILE__,__LINE__,String_cString(name),String_cString(value));
      /* find command line option */
      commandLineOption = CmdOption_find(String_cString(name),
                                         COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS)
                                        );
      if (commandLineOption == NULL)
      {
        printError("Unknown option '%s' in %s, line %ld\n",
                   String_cString(name),
                   String_cString(fileName),
                   lineNb
                  );
        failFlag = TRUE;
        break;
      }

      /* parse command line option */
      if (!CmdOption_parseString(commandLineOption,
                                 String_cString(value)
                                )
         )
      {
        printError("Error in option '%s' in %s, line %ld\n",
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
      printError("Error in %s, line %ld: %s\n",
                 String_cString(fileName),
                 lineNb,
                 String_cString(line)
                );
      failFlag = TRUE;
      break;
    }
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
* Name   : cmdParseString
* Purpose: command line option call back for parsing string
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdParseString(void *variable, const char *name, const char *value, const void *defaultValue, void *userData)
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
* Name   : cmdParseConfigFile
* Purpose: command line option call back for parsing config file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdParseConfigFile(void *variable, const char *name, const char *value, const void *defaultValue, void *userData)
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

  return TRUE;
}

/***********************************************************************\
* Name   : cmdParseIncludeExclude
* Purpose: command line option call back for parsing include/exclude
*          patterns
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdParseIncludeExclude(void *variable, const char *name, const char *value, const void *defaultValue, void *userData)
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
* Name   : cmdParsePassword
* Purpose: command line option call back for parsing password
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool cmdParsePassword(void *variable, const char *name, const char *value, const void *defaultValue, void *userData)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  Password_setCString((*(Password**)variable),value);

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

/*---------------------------------------------------------------------*/

void info(uint verboseLevel, const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  if (!defaultOptions.quietFlag && (defaultOptions.verboseLevel >= verboseLevel))
  {
    va_start(arguments,format);
    vprintf(format,arguments);
    va_end(arguments);
    fflush(stdout);
  }
}

void warning(const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  printf("Warning: ");
  va_start(arguments,format);
  vprintf(format,arguments);
  va_end(arguments);
  fflush(stdout);
}

void printError(const char *text, ...)
{
  va_list arguments;

  assert(text != NULL);

  va_start(arguments,text);
  fprintf(stderr,"ERROR: ");
  vfprintf(stderr,text,arguments);
  va_end(arguments);
}

void copyOptions(const Options *sourceOptions, Options *destinationOptions)
{
  assert(sourceOptions != NULL);
  assert(destinationOptions != NULL);

  memcpy(destinationOptions,sourceOptions,sizeof(Options));
  destinationOptions->tmpDirectory         = String_copy(sourceOptions->tmpDirectory);
  destinationOptions->directory            = String_copy(sourceOptions->directory);
  destinationOptions->cryptPassword        = Password_copy(sourceOptions->cryptPassword);
  destinationOptions->sshPublicKeyFileName = String_copy(sourceOptions->sshPublicKeyFileName);
  destinationOptions->sshPrivatKeyFileName = String_copy(sourceOptions->sshPrivatKeyFileName);
  destinationOptions->sshPassword          = Password_copy(sourceOptions->sshPassword);
  destinationOptions->remoteBARExecutable  = String_copy(sourceOptions->remoteBARExecutable);
}

void freeOptions(Options *options)
{
  assert(options != NULL);

  String_delete(options->tmpDirectory);
  String_delete(options->directory);
  if (options->cryptPassword != NULL) Password_delete(options->cryptPassword);
  String_delete(options->sshPrivatKeyFileName);
  String_delete(options->sshPublicKeyFileName);
  if (options->sshPassword != NULL) Password_delete(options->sshPassword);
  String_delete(options->remoteBARExecutable);
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  String fileName;
  Errors error;

  /* init */
  if (!initAll())
  {
    return EXITCODE_INIT_FAIL;
  }

  /* initialise variables */
  defaultOptions.cryptPassword = Password_new();
  defaultOptions.sshPassword = Password_new();
  serverPassword = Password_new();
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  Pattern_initList(&includePatternList);
  Pattern_initList(&excludePatternList);

  /* parse command line: pre-options */
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       0,
                       stderr,NULL
                      )
     )
  {
    Pattern_doneList(&excludePatternList);
    Pattern_doneList(&includePatternList);
    Password_delete(serverPassword);
    Password_delete(defaultOptions.sshPassword);
    Password_delete(defaultOptions.cryptPassword);
    doneAll();
    return EXITCODE_INVALID_ARGUMENT;
  }

  if (!defaultOptions.noDefaultConfigFlag)
  {
    fileName = String_new();

    /* read default configuration from /CONFIG_DIR/bar.cfg (ignore errors) */
    File_setFileNameCString(fileName,CONFIG_DIR);
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    readConfigFile(fileName,FALSE);

    /* read default configuration from $HOME/.bar/bar.cfg (if exists) */
    File_setFileNameCString(fileName,getenv("HOME"));
    File_appendFileNameCString(fileName,".bar");
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_exists(fileName))
    {
      if (!readConfigFile(fileName,TRUE))
      {
        String_delete(fileName);
        #ifndef NDEBUG
          String_debug();
        #endif /* not NDEBUG */
        Pattern_doneList(&excludePatternList);
        Pattern_doneList(&includePatternList);
        Password_delete(serverPassword);
        Password_delete(defaultOptions.sshPassword);
        Password_delete(defaultOptions.cryptPassword);
        doneAll();
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
    Pattern_doneList(&excludePatternList);
    Pattern_doneList(&includePatternList);
    Password_delete(serverPassword);
    Password_delete(defaultOptions.sshPassword);
    Password_delete(defaultOptions.cryptPassword);
    doneAll();
    return EXITCODE_INVALID_ARGUMENT;
  }
  if (versionFlag)
  {
    printf("BAR version %s\n",VERSION);
    Pattern_doneList(&excludePatternList);
    Pattern_doneList(&includePatternList);
    Password_delete(serverPassword);
    Password_delete(defaultOptions.sshPassword);
    Password_delete(defaultOptions.cryptPassword);
    doneAll();
    return EXITCODE_OK;
  }
  if (helpFlag || xhelpFlag || helpInternalFlag)
  {
    if      (helpInternalFlag) printUsage(argv[0],2);
    else if (xhelpFlag       ) printUsage(argv[0],1);
    else                       printUsage(argv[0],0);

    Pattern_doneList(&excludePatternList);
    Pattern_doneList(&includePatternList);
    Password_delete(serverPassword);
    Password_delete(defaultOptions.sshPassword);
    Password_delete(defaultOptions.cryptPassword);
    doneAll();
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
  freeOptions(&defaultOptions);
  CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  Pattern_doneList(&excludePatternList);
  Pattern_doneList(&includePatternList);
  Password_delete(serverPassword);
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
