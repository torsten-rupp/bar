/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.c,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents: Backup ARchiver main program
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "global.h"
#include "cmdoptions.h"

#include "errors.h"
#include "files.h"
#include "patterns.h"
#include "crypt.h"
#include "archive.h"
#include "command_create.h"
#include "command_list.h"
#include "command_restore.h"
#include "command_test.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define VERSION "0.01"

/***************************** Datatypes *******************************/

typedef enum
{
  COMMAND_NONE,

  COMMAND_CREATE,
  COMMAND_LIST,
  COMMAND_TEST,
  COMMAND_RESTORE,

  COMMAND_UNKNOWN,
} Commands;

/***************************** Variables *******************************/

GlobalOptions globalOptions;

LOCAL Commands           command;
LOCAL const char         *archiveFileName;
LOCAL uint               directoryStripCount;
LOCAL const char         *directory;
LOCAL ulong              partSize;
LOCAL PatternTypes       patternType;
LOCAL PatternList        includePatternList;
LOCAL PatternList        excludePatternList;
LOCAL CompressAlgorithms compressAlgorithm;
LOCAL CryptAlgorithms    cryptAlgorithm;
LOCAL const char         *password;
LOCAL bool               versionFlag;
LOCAL bool               helpFlag;

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_PATTERN_TYPE[] =
{
  {"glob",  PATTERN_TYPE_GLOB,    "glob patterns: * and ?"},
  {"basic", PATTERN_TYPE_BASIC,   "basic pattern matching"},
  {"extend",PATTERN_TYPE_EXTENDED,"extended pattern matching"},
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHM[] =
{
  {"none",COMPRESS_ALGORITHM_NONE,"no compression"},
  {"zip0",COMPRESS_ALGORITHM_ZIP0,"ZIP compression level 0"},
  {"zip1",COMPRESS_ALGORITHM_ZIP1,"ZIP compression level 1"},
  {"zip2",COMPRESS_ALGORITHM_ZIP2,"ZIP compression level 2"},
  {"zip3",COMPRESS_ALGORITHM_ZIP3,"ZIP compression level 3"},
  {"zip4",COMPRESS_ALGORITHM_ZIP4,"ZIP compression level 4"},
  {"zip5",COMPRESS_ALGORITHM_ZIP5,"ZIP compression level 5"},
  {"zip6",COMPRESS_ALGORITHM_ZIP6,"ZIP compression level 6"},
  {"zip7",COMPRESS_ALGORITHM_ZIP7,"ZIP compression level 7"},
  {"zip8",COMPRESS_ALGORITHM_ZIP8,"ZIP compression level 8"},
  {"zip9",COMPRESS_ALGORITHM_ZIP9,"ZIP compression level 9"},
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_CRYPT_ALGORITHM[] =
{
  {"none",  CRYPT_ALGORITHM_NONE,  "no crypting"},
  {"aes128",CRYPT_ALGORITHM_AES128,"AES cipher 128bit"},
};

LOCAL bool parseIncludeExclude(void *variable, const char *value, const void *defaultValue, void *userData)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(userData);

  return Patterns_addList((PatternList*)variable,value,patternType) == ERROR_NONE;
}

LOCAL const CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM   ("create",         'c',0,command,                   COMMAND_NONE,COMMAND_CREATE,                                    "create new archive"), 
  CMD_OPTION_ENUM   ("list",           'l',0,command,                   COMMAND_NONE,COMMAND_LIST,                                      "list contents of archive"), 
  CMD_OPTION_ENUM   ("test",           't',0,command,                   COMMAND_NONE,COMMAND_TEST,                                      "test contents of ardhive"), 
  CMD_OPTION_ENUM   ("extract",        'x',0,command,                   COMMAND_NONE,COMMAND_RESTORE,                                   "restore archive"), 

  CMD_OPTION_STRING ("archive",        'a',0,archiveFileName,           NULL,                                                           "archive filename"),
  CMD_OPTION_INTEGER("part-size",      's',0,partSize,                  0,                                                              "part size"),
  CMD_OPTION_STRING ("tmp-directory",  0,  0,globalOptions.tmpDirectory,"/tmp",                                                         "temporary directory"),
  CMD_OPTION_INTEGER("directory-strip",'p',0,directoryStripCount,       0,                                                              "number of directories to strip on extract"),
  CMD_OPTION_STRING ("directory",      0,  0,directory,                 NULL,                                                           "directory to restore files"),

  CMD_OPTION_SELECT ("pattern-type",   0,  0,patternType,               PATTERN_TYPE_GLOB,COMMAND_LINE_OPTIONS_PATTERN_TYPE,            "select pattern type"),

  CMD_OPTION_SPECIAL("include",        'i',1,includePatternList,        NULL,parseIncludeExclude,NULL,                                  "include pattern"),
  CMD_OPTION_SPECIAL("exclude",        0,  1,excludePatternList,        NULL,parseIncludeExclude,NULL,                                  "exclude pattern"),

  CMD_OPTION_SELECT ("compress",       0,  0,compressAlgorithm,         COMPRESS_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_COMPRESS_ALGORITHM,"select compress algorithm to use"),

  CMD_OPTION_SELECT ("crypt",          0,  0,cryptAlgorithm,            CRYPT_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_CRYPT_ALGORITHM,      "select crypt algorithm to use"),
  CMD_OPTION_STRING ("password",       0,  0,password,                  NULL,                                                           "crypt password"),

  CMD_OPTION_BOOLEAN("quiet",          'q',0,globalOptions.quietFlag,   FALSE,                                                          "surpress any output"),
  CMD_OPTION_INTEGER("verbose",        'v',0,globalOptions.verboseLevel,0,                                                              "verbosity level"),

  CMD_OPTION_BOOLEAN("version",        0  ,0,versionFlag,               FALSE,                                                          "print version"),
  CMD_OPTION_BOOLEAN("help",           'h',0,helpFlag,                  FALSE,                                                          "print this help"),
};

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

void info(uint verboseLevel, const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  if (!globalOptions.quietFlag && (globalOptions.verboseLevel>=verboseLevel))
  {
    va_start(arguments,format);
    vprintf(format,arguments);
    va_end(arguments);
    fflush(stdout);
  }
}

const char *getErrorText(Errors error)
{
  #define CASE(error,text) case error: return text; break;
  #define DEFAULT(text) default: return text; break;

  switch (error)
  {
    CASE(ERROR_NONE,                   "none"                  );
                                                  
    CASE(ERROR_INSUFFICIENT_MEMORY,    "insufficient memory"   );
    CASE(ERROR_INIT,                   "init"                  );

    CASE(ERROR_INVALID_PATTERN,        "init pattern matching" );

    CASE(ERROR_INIT_COMPRESS,          "init compress"         );
    CASE(ERROR_COMPRESS_ERROR,         "compress"              );

    CASE(ERROR_UNSUPPORTED_BLOCK_SIZE, "unsupported block size");
    CASE(ERROR_INIT_CRYPT,             "init crypt"            );
    CASE(ERROR_NO_PASSWORD,            "no password"           );
    CASE(ERROR_INIT_CIPHER,            "init cipher"           );
    CASE(ERROR_ENCRYPT_FAIL,           "encrypt"               );
    CASE(ERROR_DECRYPT_FAIL,           "decrypt"               );

    CASE(ERROR_CREATE_FILE,            "create file"           );
    CASE(ERROR_OPEN_FILE,              "open file"             );
    CASE(ERROR_OPEN_DIRECTORY,         "open directory"        );
    CASE(ERROR_IO_ERROR,               "input/output"          );

    CASE(ERROR_END_OF_ARCHIVE,         "end of archive"        );
    CASE(ERROR_NO_FILE_ENTRY,          "no file entry"         );
    CASE(ERROR_NO_FILE_DATA,           "no data entry"         );
    CASE(ERROR_END_OF_DATA,            "end of data"           );

    DEFAULT(                           "unknown"               );
  }

  #undef DEFAULT
  #undef CASE
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

/***********************************************************************\
* Name       : PrintUsage
* Purpose    : print "usage" help
* Input      : -
* Output     : -
* Return     : -
* Side-effect: unknown
* Notes      : -
\***********************************************************************/

LOCAL void printUsage(const char *programName)
{
  assert(programName != NULL);

  printf("Usage: %s [<options>] [--] <archive name>... [<files>...]\n",programName);
  printf("\n");
  cmdOptions_printHelp(stdout,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS)
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

LOCAL bool init(void)
{
  Errors error;

  error = Crypt_init();
  if (error != ERROR_NONE)
  {
    return FALSE;
  }
  error = Patterns_init();
  if (error != ERROR_NONE)
  {
    Crypt_done();
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

LOCAL void done(void)
{
  Patterns_done();
  Crypt_done();
}

/***********************************************************************\
* Name   : freeFileNameNode
* Purpose: free allocated filename node
* Input  : fileNameNode - filename node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFileNameNode(FileNameNode *fileNameNode, void *userData)
{
  assert(fileNameNode != NULL);

  String_delete(((FileNameNode*)fileNameNode)->fileName);
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
 {
  int          z;
  FileNameList fileNameList;
  FileNameNode *fileNameNode;
  Errors       error;
  int          exitcode;

  /* init */
  if (!init())
  {
    exit(EXITCODE_INIT_FAIL);
  }

  /* initialise variables */
  Patterns_newList(&includePatternList);
  Patterns_newList(&excludePatternList);

  /* parse command line */
  if (!cmdOptions_parse(argv,&argc,
                        COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                        stderr,NULL
                       )
     )
  {
    return EXITCODE_INVALID_ARGUMENT;
  }
  if (versionFlag)
  {
    printf("BAR version %s\n",VERSION);
    return EXITCODE_OK;
  }
  if (helpFlag)
  {
    printUsage(argv[0]);
    return EXITCODE_OK;
  }

  exitcode = EXITCODE_UNKNOWN;
  switch (command)
  {
    case COMMAND_CREATE:
      {
        /* check command line arguments */
        if (archiveFileName == NULL)
        {
          printError("no archive filename given!\n");
          return EXITCODE_INVALID_ARGUMENT;
        }

        /* get include patterns */
        for (z = 1; z < argc; z++)
        {
          error = Patterns_addList(&includePatternList,argv[z],patternType);
        }

        /* create archive */
        exitcode = (command_create(archiveFileName,
                                   &includePatternList,
                                   &excludePatternList,
                                   globalOptions.tmpDirectory,
                                   partSize,
                                   compressAlgorithm,
                                   cryptAlgorithm,
                                   password
                                  )
                   )?EXITCODE_OK:EXITCODE_FAIL;
      }
      break;
    case COMMAND_LIST:
    case COMMAND_TEST:
    case COMMAND_RESTORE:
      {
        /* get archive files */
        Lists_init(&fileNameList);
        for (z = 1; z < argc; z++)
        {
          fileNameNode = (FileNameNode*)malloc(sizeof(FileNameNode));
          if (fileNameNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          fileNameNode->fileName = String_newCString(argv[z]);
          Lists_add(&fileNameList,fileNameNode);
        }

        switch (command)
        {
          case COMMAND_LIST:
            exitcode = (command_list(&fileNameList,
                                     &includePatternList,
                                     &excludePatternList,
                                     password
                                    )
                       )?EXITCODE_OK:EXITCODE_FAIL;
            break;
          case COMMAND_TEST:
            exitcode = (command_test(&fileNameList,
                                     &includePatternList,
                                     &excludePatternList,
                                     password
                                    )
                       )?EXITCODE_OK:EXITCODE_FAIL;
            break;
          case COMMAND_RESTORE:
            exitcode = (command_restore(&fileNameList,
                                        &includePatternList,
                                        &excludePatternList,
                                        directoryStripCount,
                                        directory,
                                        password
                                       )
                       )?EXITCODE_OK:EXITCODE_FAIL;
            break;
          default:
            break;
        }

        /* free resources */
        Lists_done(&fileNameList,(NodeFreeFunction)freeFileNameNode,NULL);
      }
      break;
    default:
      printError("No command given!\n");
      return EXITCODE_INVALID_ARGUMENT;
      break;
  }

  /* free resources */
  Patterns_deleteList(&excludePatternList);
  Patterns_deleteList(&includePatternList);

  /* done */
  done();

  return exitcode;
 }

#ifdef __cplusplus
  }
#endif

/* end of file */
