/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/destroyer.c,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: "destroy" binary files by overwrite/insert/delete bytes;
*           used for test only!
* Systems:
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "cmdoptions.h"
#include "strings.h"

#include "destroyer.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define POSITION_RANDOM -1

typedef enum
{
  DEFINITION_TYPE_MODIFY,
  DEFINITION_TYPE_INSERT,
  DEFINITION_TYPE_DELETE,
} DefinitionTypes;

/***************************** Datatypes *******************************/

typedef struct
{
  DefinitionTypes type;
  int             value;
  int64           position;
} Definition;

/***************************** Variables *******************************/

LOCAL bool versionFlag = FALSE;
LOCAL bool helpFlag    = FALSE;

LOCAL CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_BOOLEAN("version",0  ,0,0,versionFlag,"print version"  ),
  CMD_OPTION_BOOLEAN("help",   'h',0,0,helpFlag,   "print this help"),
};

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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

  printf("Usage: %s [<options>] [--] <input file> [<output file>]\n",programName);
  printf("\n");
  CmdOption_printHelp(stdout,
                      COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                      0
                     );
 }

/***********************************************************************\
* Name   : initRandom
* Purpose: init random generator
* Input  : seed - seed value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initRandom(int64 seed)
{
  srand((unsigned int)seed);
}

/***********************************************************************\
* Name   : getRandomInt64
* Purpose: get random byte value
* Input  : -
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

LOCAL byte getRandomByte(uint max)
{
  return (byte)rand()%max;
}

/***********************************************************************\
* Name   : getRandomInt64
* Purpose: get random int64 value
* Input  : max - max. value
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

LOCAL int64 getRandomInt64(int64 max)
{
  return (int64)(((uint64)rand() << 32) | ((uint64)rand() << 0))%max;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool parseDefinition(const char *s, Definition *definition, uint64 maxPosition)
{
  StringTokenizer stringTokenizer;
  String          t;
  String          w;

  assert(definition != NULL);

  t = String_newCString(s);
  String_initTokenizer(&stringTokenizer,t,STRING_BEGIN,":",NULL,FALSE);

  /* get type */
  if (String_getNextToken(&stringTokenizer,&w,NULL))
  {
    if      (String_equalsCString(w,"m")) definition->type = DEFINITION_TYPE_MODIFY;
    else if (String_equalsCString(w,"i")) definition->type = DEFINITION_TYPE_INSERT;
    else if (String_equalsCString(w,"d")) definition->type = DEFINITION_TYPE_DELETE;
    else
    {
      String_doneTokenizer(&stringTokenizer);
      String_delete(t);
      fprintf(stderr,"ERROR: Invalid definition '%s': expected m,i,d!\n",s);
      return FALSE;
    }
  }
  else
  {
    String_doneTokenizer(&stringTokenizer);
    String_delete(t);
    fprintf(stderr,"ERROR: Invalid definition '%s'!\n",s);
    return FALSE;
  }

  /* get position */
  if (String_getNextToken(&stringTokenizer,&w,NULL))
  {
    if (!String_scan(w,STRING_BEGIN,"%llu",&definition->position))
    {
      String_doneTokenizer(&stringTokenizer);
      String_delete(t);
      fprintf(stderr,"ERROR: Invalid position in definition '%s'!\n",s);
      return FALSE;
    }
  }
  else
  {
    definition->position = getRandomInt64(maxPosition);
  }

  /* get value */
  if (String_getNextToken(&stringTokenizer,&w,NULL))
  {
    if (!String_scan(w,STRING_BEGIN,"%c",&definition->value))
    {
      String_doneTokenizer(&stringTokenizer);
      String_delete(t);
      fprintf(stderr,"ERROR: Invalid value in definition '%s'!\n",s);
      return FALSE;
    }
  }
  else
  {
    definition->value = getRandomByte(256);
  }

  if (String_getNextToken(&stringTokenizer,&w,NULL))
  {
    String_doneTokenizer(&stringTokenizer);
    String_delete(t);
    fprintf(stderr,"ERROR: Invalid definition '%s'!\n",s);
    return FALSE;
  }

  String_doneTokenizer(&stringTokenizer);
  String_delete(t);

  return TRUE;
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
 {
  Definition  definition;
  Definition  definitions[MAX_DEFINITIONS];
  uint        definitionCount;
  int         z;
  struct stat statBuffer;
  uint64      size;
  const char  *inputFileName;
  FILE        *inputHandle;
  byte        data;
  uint64      n;

  /* parse command line */
  CmdOption_init(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS),
                       CMD_PRIORITY_ANY,
                       stderr,NULL
                      )
     )
  {
    return 1;
  }
  if (versionFlag)
  {
    printf("Destroyer version %s\n",VERSION);
    return 0;
  }
  if (helpFlag)
  {
    printUsage(argv[0]);
    return 0;
  }
  if (argc <= 1)
  {
    fprintf(stderr,"ERROR: No input file given!\n");
    exit(1);
  }
  inputFileName = argv[1];

  /* get file size */
  if (stat(inputFileName,&statBuffer) != 0)
  {
    fprintf(stderr,"ERROR: Cannot detect size of file '%s' (error: %s)\n",
            inputFileName,
            strerror(errno)
           );
    exit(1);
  }
  size = statBuffer.st_size;

  /* parse definitions */
  initRandom(size);
  definitionCount = 0;
  for (z = 2; z < argc; z++)
  {
    if (!parseDefinition(argv[z],&definition,size))
    {
      exit(1);
    }
    if (definitionCount >= MAX_DEFINITIONS)
    {
      fprintf(stderr,"ERROR: to many definitions! Max. %d possible.\n",MAX_DEFINITIONS);
    }
    definitions[definitionCount] = definition;
    definitionCount++;
  }

  /* open input file */
  inputHandle = fopen(inputFileName,"r");
  if (inputHandle == NULL)
  {
    fprintf(stderr,"ERROR: Cannot open file '%s' (error: %s)\n",
            inputFileName,
            strerror(errno)
           );
    exit(1);
  }

  /* destroy and write to stdout */
  for (n = 0; n < size; n++)
  {
    /* read byte */
    data = fgetc(inputHandle);

    /* find matching definition */
    z = 0;
    while ((z < definitionCount) && (n != definitions[z].position))
    {
      z++;
    }

    /* output byte */
    if (z < definitionCount)
    {
      switch (definitions[z].type)
      {
        case DEFINITION_TYPE_MODIFY:
          fputc(definitions[z].value,stdout);
          break;
        case DEFINITION_TYPE_INSERT:
          fputc(definitions[z].value,stdout);
          fputc(data,stdout);
          break;
        case DEFINITION_TYPE_DELETE:
          break;
      }
    }
    else
    {
      fputc(data,stdout);
    }
  }

  /* close files */
  fclose(inputHandle);

  /* free resources */
  CmdOption_done(COMMAND_LINE_OPTIONS,SIZE_OF_ARRAY(COMMAND_LINE_OPTIONS));

  return 0;
 }

#ifdef __cplusplus
  }
#endif

/* end of file */
