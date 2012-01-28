/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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
  DEFINITION_TYPE_RANDOMIZE,
  DEFINITION_TYPE_INSERT,
  DEFINITION_TYPE_DELETE,
} DefinitionTypes;

/***************************** Datatypes *******************************/

typedef struct
{
  DefinitionTypes type;
  int64           position;
  union
  {
    String value;
    uint   length;
  };
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

  printf("Usage: %s [<options>] [--] <input file> [<command>...]\n",programName);
  printf("\n");
  printf("Commands:\n");
  printf("  m:<position>:<value>  - modify byte at <position> into <value>\n");
  printf("  r:<position>:<length> - randomize <length> byte from <position>\n");
  printf("  d:<position>:<length> - delete <length> bytes from <position>\n");
  printf("  i:<position>:<value>  - insert at <position> byte <value>\n");
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
* Name   : getRandomByte
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
* Name   : getRandomInteger
* Purpose: get random int value
* Input  : -
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

LOCAL uint getRandomInteger(uint max)
{
  return (uint)rand()%max;
}

/***********************************************************************\
* Name   : getRandomInteger64
* Purpose: get random int64 value
* Input  : max - max. value
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

LOCAL uint64 getRandomInteger64(uint64 max)
{
  return (int64)(((uint64)rand() << 32) | ((uint64)rand() << 0))%max;
}

/***********************************************************************\
* Name   : getRandomBuffer
* Purpose: get random buffer
* Input  : buffer - buffer to fill
*          length - length of buffer
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

LOCAL void getRandomBuffer(void *buffer, uint length)
{
  byte *p;

  p = (byte*)buffer;
  while (length > 0)
  {
    (*p) = (byte)(rand()%256);
    p++;
    length--;
  }
}

/***********************************************************************\
* Name   : parseDefinition
* Purpose: parse definitions
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
  uint            length;
  char            buffer[1024];

  assert(definition != NULL);

  t = String_newCString(s);
  String_initTokenizer(&stringTokenizer,t,STRING_BEGIN,":",NULL,FALSE);

  /* get type */
  if (String_getNextToken(&stringTokenizer,&w,NULL))
  {
    if      (String_equalsCString(w,"m")) definition->type = DEFINITION_TYPE_MODIFY;
    else if (String_equalsCString(w,"r")) definition->type = DEFINITION_TYPE_RANDOMIZE;
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
    definition->position = getRandomInteger64(maxPosition);
  }

  /* get value/length */
  if (String_getNextToken(&stringTokenizer,&w,NULL))
  {
    switch (definition->type)
    {
      case DEFINITION_TYPE_MODIFY:
        definition->value = String_new();
        if (!String_scan(w,STRING_BEGIN,"%S",definition->value))
        {
          String_delete(definition->value);
          String_doneTokenizer(&stringTokenizer);
          String_delete(t);
          fprintf(stderr,"ERROR: Invalid length in definition '%s'!\n",s);
          return FALSE;
        }
        break;
      case DEFINITION_TYPE_RANDOMIZE:
        if (!String_scan(w,STRING_BEGIN,"%u",&length))
        {
          String_doneTokenizer(&stringTokenizer);
          String_delete(t);
          fprintf(stderr,"ERROR: Invalid length in definition '%s'!\n",s);
          return FALSE;
        }
        if (length > sizeof(buffer)) length = sizeof(buffer);
        getRandomBuffer(buffer,length);
        definition->value = String_newBuffer(buffer,length);
        break;
      case DEFINITION_TYPE_INSERT:
        definition->value = String_new();
        if (!String_scan(w,STRING_BEGIN,"%S",definition->value))
        {
          String_delete(definition->value);
          String_doneTokenizer(&stringTokenizer);
          String_delete(t);
          fprintf(stderr,"ERROR: Invalid value in definition '%s'!\n",s);
          return FALSE;
        }
        break;
      case DEFINITION_TYPE_DELETE:
        if (!String_scan(w,STRING_BEGIN,"%u",&definition->length))
        {
          String_doneTokenizer(&stringTokenizer);
          String_delete(t);
          fprintf(stderr,"ERROR: Invalid length in definition '%s'!\n",s);
          return FALSE;
        }
        break;
    }
  }
  else
  {
    switch (definition->type)
    {
      case DEFINITION_TYPE_MODIFY:
        definition->value = String_newChar((char)getRandomByte(256));
        break;
      case DEFINITION_TYPE_RANDOMIZE:
        getRandomBuffer(buffer,sizeof(buffer));
        definition->value = String_newBuffer(buffer,getRandomInteger(sizeof(buffer)));
        break;
      case DEFINITION_TYPE_INSERT:
        definition->value = String_new();
        if (!String_scan(w,STRING_BEGIN,"%S",&definition->value))
        {
          String_delete(definition->value);
          String_doneTokenizer(&stringTokenizer);
          String_delete(t);
          fprintf(stderr,"ERROR: Invalid value in definition '%s'!\n",s);
          return FALSE;
        }
        break;
      case DEFINITION_TYPE_DELETE:
        if (!String_scan(w,STRING_BEGIN,"%u",&definition->length))
        {
          String_doneTokenizer(&stringTokenizer);
          String_delete(t);
          fprintf(stderr,"ERROR: Invalid length in definition '%s'!\n",s);
          return FALSE;
        }
        break;
    }
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
  uint        deleteCount;
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
  deleteCount = 0;
  for (n = 0; n < size; n++)
  {
    /* read byte */
    data = fgetc(inputHandle);

    if (deleteCount == 0)
    {
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
          case DEFINITION_TYPE_RANDOMIZE:
            fwrite(String_cString(definitions[z].value),1,String_length(definitions[z].value),stdout);
            deleteCount = String_length(definitions[z].value)-1;
            break;
          case DEFINITION_TYPE_INSERT:
            fwrite(String_cString(definitions[z].value),1,String_length(definitions[z].value),stdout);
            fputc(data,stdout);
            break;
          case DEFINITION_TYPE_DELETE:
            deleteCount = definitions[z].length-1;
            break;
        }
      }
      else
      {
        fputc(data,stdout);
      }
    }
    else
    {
      deleteCount--;
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
