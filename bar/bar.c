/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.c,v $
* $Revision: 1.1.1.1 $
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

#include "bar.h"
#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  COMMAND_NONE,

  COMMAND_CREATE,
  COMMAND_LIST,
  COMMAND_TEST,
  COMMAND_EXTRACT,

  COMMAND_UNKNOWN,
} Commands;

/***************************** Variables *******************************/
LOCAL Commands      command;
LOCAL const char    *archiveFileName;
LOCAL unsigned long partSize;
LOCAL const char    *tmpDirectory;
LOCAL const char    *directory;
LOCAL bool          helpFlag;

LOCAL const CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_ENUM   ("create",       'c',command,        COMMAND_NONE,COMMAND_CREATE, "create new archive"), 
  CMD_OPTION_ENUM   ("list",         'l',command,        COMMAND_NONE,COMMAND_LIST,   "list contents of archive"), 
  CMD_OPTION_ENUM   ("test",         't',command,        COMMAND_NONE,COMMAND_TEST,   "test contents of ardhive"), 
  CMD_OPTION_ENUM   ("extract",      'x',command,        COMMAND_NONE,COMMAND_EXTRACT,"restore archive"), 

  CMD_OPTION_STRING ("archive",      'a',archiveFileName,NULL,                        "archive filename"),
  CMD_OPTION_INTEGER("part-size",    's',partSize,       0,                           "part size"),
  CMD_OPTION_STRING ("tmp-directory",0,  tmpDirectory,   "/tmp",                      "temporary directory"),
  CMD_OPTION_STRING ("directory",    0,  directory,      NULL,                        "directory to restore files"),

  CMD_OPTION_BOOLEAN("help",         'h',helpFlag,       FALSE,                       "print this help"),
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
* Name   : freePatternNode
* Purpose: free allocated pattern node
* Input  : patterNode - pattern node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freePatternNode(PatternNode *patternNode, void *userData)
{
  assert(patternNode != NULL);

  String_delete(((PatternNode*)patternNode)->pattern);
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
  PatternList  includePatternList;
  PatternList  excludePatternList;
  PatternNode  *patternNode;
  FileNameList fileNameList;
  FileNameNode *fileNameNode;
  int          exitcode;

  /* initialise variables */
  List_init(&includePatternList);
  List_init(&excludePatternList);

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
          patternNode = (PatternNode*)malloc(sizeof(PatternNode));
          if (patternNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          patternNode->pattern = String_newCString(argv[z]);
          List_add(&includePatternList,patternNode);
        }

        /* create archive */
        exitcode = (archive_create(archiveFileName,&includePatternList,&excludePatternList,tmpDirectory,partSize))?EXITCODE_OK:EXITCODE_FAIL;
      }
      break;
    case COMMAND_LIST:
    case COMMAND_TEST:
    case COMMAND_EXTRACT:
      {
        /* get archive files */
        List_init(&fileNameList);
        for (z = 1; z < argc; z++)
        {
          fileNameNode = (FileNameNode*)malloc(sizeof(FileNameNode));
          if (fileNameNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          fileNameNode->fileName = String_newCString(argv[z]);
          List_add(&fileNameList,fileNameNode);
        }

        switch (command)
        {
          case COMMAND_LIST:
            exitcode = (archive_list(&fileNameList,&includePatternList,&excludePatternList))?EXITCODE_OK:EXITCODE_FAIL;
            break;
          case COMMAND_TEST:
            archive_test(&fileNameList,&includePatternList,&excludePatternList);
            break;
          case COMMAND_EXTRACT:
            archive_restore(&fileNameList,&includePatternList,&excludePatternList,directory);
            break;
          default:
            break;
        }

        /* free resources */
        List_done(&fileNameList,(NodeFreeFunction)freeFileNameNode,NULL);
      }
      break;
    default:
      printError("No command given!\n");
      return EXITCODE_INVALID_ARGUMENT;
      break;
  }

  /* free resources */
  List_done(&excludePatternList,(NodeFreeFunction)freePatternNode,NULL);
  List_done(&includePatternList,(NodeFreeFunction)freePatternNode,NULL);

  return exitcode;
 }

#ifdef __cplusplus
  }
#endif

/* end of file */
