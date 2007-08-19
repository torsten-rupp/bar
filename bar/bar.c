/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.c,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: Backup ARchiver
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
LOCAL Commands        command;
LOCAL const char      *archiveFileName;
LOCAL unsigned long   partSize;
LOCAL const char      *tmpDirectory;
LOCAL uint            directoryStripCount;
LOCAL const char      *directory;
LOCAL PatternTypes    patternType;
LOCAL PatternList     includePatternList;
LOCAL PatternList     excludePatternList;
LOCAL CryptAlgorithms cryptAlgorithm;
LOCAL const char      *password;
LOCAL bool            helpFlag;

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_PATTERN_TYPE[] =
{
  {"glob",  PATTERN_TYPE_GLOB,    "glob patterns: * and ?"},
  {"basic", PATTERN_TYPE_BASIC,   "basic pattern matching"},
  {"extend",PATTERN_TYPE_EXTENDED,"extended pattern matching"},
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
  CMD_OPTION_ENUM   ("create",         'c',0,command,            COMMAND_NONE,COMMAND_CREATE,                        "create new archive"), 
  CMD_OPTION_ENUM   ("list",           'l',0,command,            COMMAND_NONE,COMMAND_LIST,                          "list contents of archive"), 
  CMD_OPTION_ENUM   ("test",           't',0,command,            COMMAND_NONE,COMMAND_TEST,                          "test contents of ardhive"), 
  CMD_OPTION_ENUM   ("extract",        'x',0,command,            COMMAND_NONE,COMMAND_RESTORE,                       "restore archive"), 

  CMD_OPTION_STRING ("archive",        'a',0,archiveFileName,    NULL,                                               "archive filename"),
  CMD_OPTION_INTEGER("part-size",      's',0,partSize,           0,                                                  "part size"),
  CMD_OPTION_STRING ("tmp-directory",  0,  0,tmpDirectory,       "/tmp",                                             "temporary directory"),
  CMD_OPTION_INTEGER("directory-strip",'p',0,directoryStripCount,0,                                                  "number of directories to strip on extract"),
  CMD_OPTION_STRING ("directory",      0,  0,directory,          NULL,                                               "directory to restore files"),

  CMD_OPTION_SELECT ("pattern-type",   0,  0,patternType,        PATTERN_TYPE_GLOB,COMMAND_LINE_OPTIONS_PATTERN_TYPE,"select pattern type"),

  CMD_OPTION_SPECIAL("include",        'i',1,includePatternList, NULL,parseIncludeExclude,NULL,                      "include pattern"),
  CMD_OPTION_SPECIAL("exclude",        0,  1,excludePatternList, NULL,parseIncludeExclude,NULL,                      "exclude pattern"),

  CMD_OPTION_SELECT ("crypt-algorithm",0,  0,cryptAlgorithm,     CRYPT_ALGORITHM_NONE,COMMAND_LINE_OPTIONS_CRYPT_ALGORITHM,"select crypt algorithm to use"),
  CMD_OPTION_STRING ("password",       0,  0,password,           NULL,                                                     "crypt password"),


  CMD_OPTION_BOOLEAN("help",           'h',0,helpFlag,           FALSE,                                              "print this help"),
};

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printError(const char *text, ...)
{
  va_list arguments;

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
  printf("Usage: %s [<options>] [--] [cltx]...\n",programName);
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
* Return : -
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
                                   tmpDirectory,
                                   partSize,
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
