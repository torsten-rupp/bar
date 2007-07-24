/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/cmdoptions.c,v $
* $Revision: 1.1.1.1 $
* $Author: torsten $
* Contents: command line options parser
* Systems :
*
***********************************************************************/

/****************************** Includes ******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "cmdoptions.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/

/***************************** Datatypes ******************************/

/***************************** Variables ******************************/

/******************************* Macros *******************************/

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************\
* Name   : processOption
* Purpose: process single command line option
* Input  : commandLineOption - command line option
*          prefix            - option prefix ("-" or "--")
*          name              - option name
*          value             - option value or NULL
*          errorOutputHandle - error output handle or NULL
*          errorPrefix       - error prefix or NULL
* Output : -
* Return : TRUE if option processed without error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool processOption(const CommandLineOption *commandLineOption,
                         const char              *prefix,
                         const char              *name,
                         const char              *value,
                         FILE                    *errorOutputHandle,
                         const char              *errorPrefix
                        )
{
  uint i;

  assert(commandLineOption != NULL);

  switch (commandLineOption->type)
  {
    case CMD_OPTION_TYPE_INTEGER:
      assert(commandLineOption->variable.n != NULL);
      (*commandLineOption->variable.n) = strtol(value,0,0);
      break;
    case CMD_OPTION_TYPE_DOUBLE:
      assert(commandLineOption->variable.d != NULL);
      (*commandLineOption->variable.d) = strtod(value,0);
      break;
    case CMD_OPTION_TYPE_BOOLEAN:
      assert(commandLineOption->variable.b != NULL);
      if      (   (value == NULL)
               || (strcmp(value,"1") == 0)
               || (strcmp(value,"true") == 0)
               || (strcmp(value,"on") == 0)
              )
      {
        (*commandLineOption->variable.b) = TRUE;
      }
      else if (   (strcmp(value,"0") == 0)
               || (strcmp(value,"false") == 0)
               || (strcmp(value,"off") == 0)
              )
      {
        (*commandLineOption->variable.b) = FALSE;
      }
      else
      {
        if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"Invalid value '%s' for boolean option '%s%s'!\n",value,prefix,name);
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_ENUM:
      assert(commandLineOption->variable.enumeration != NULL);
      (*commandLineOption->variable.enumeration) = commandLineOption->enumerationValue;
      break;
    case CMD_OPTION_TYPE_SELECT:
      assert(commandLineOption->variable.enumeration != NULL);
      i = 0;
      while ((i < commandLineOption->selectCount) && (strcmp(commandLineOption->selects[i].name,value) != 0))
      {
        i++;
      }
      if (i >= commandLineOption->selectCount)
      {
        if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"Unknown value '%s' for option '%s%s'!\n",value,prefix,name);
        return FALSE;
      }
      (*commandLineOption->variable.enumeration) = commandLineOption->selects[i].value;
      break;
    case CMD_OPTION_TYPE_STRING:
      assert(commandLineOption->variable.string != NULL);
      (*commandLineOption->variable.string) = value;
      break;

    case CMD_OPTION_TYPE_SPECIAL:
      assert(commandLineOption->variable.parseSpecial != NULL);
      if (!commandLineOption->variable.parseSpecial(commandLineOption->userData, value, commandLineOption->defaultValue.p))
      {
        if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"Cannot parse value '%s' for option '%s%s'!\n",value,prefix,name);
        return FALSE;
      }
      break;
  }

  return TRUE;
}

bool cmdOptions_parse(const char              *argv[],
                      int                     *argc,
                      const CommandLineOption commandLineOptions[],
                      uint                    commandLineOptionCount,
                      FILE                    *errorOutputHandle,
                      const char              *errorPrefix
                     )
{
  int        argumentsCount;
  bool       endOfOptionsFlag;
  uint       z;
  const char *s;
  char       name[128];
  const char *optionChars;
  const char *value;
  uint       i;

  assert(argv != NULL);
  assert(argc != NULL);
  assert((*argc) >= 1);
  assert(commandLineOptions != NULL);

  argumentsCount = 1;

  /* set default values */
  for (i = 0; i < commandLineOptionCount; i++)
  {
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
        assert(commandLineOptions[i].variable.n != NULL);
        (*commandLineOptions[i].variable.n) = commandLineOptions[i].defaultValue.n;
        break;
      case CMD_OPTION_TYPE_DOUBLE:
        assert(commandLineOptions[i].variable.d != NULL);
        (*commandLineOptions[i].variable.d) = commandLineOptions[i].defaultValue.d;
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        assert(commandLineOptions[i].variable.b != NULL);
        (*commandLineOptions[i].variable.b) = commandLineOptions[i].defaultValue.b;
        break;
      case CMD_OPTION_TYPE_ENUM:
        assert(commandLineOptions[i].variable.enumeration != NULL);
        (*commandLineOptions[i].variable.enumeration) = commandLineOptions[i].defaultValue.enumeration;
        break;
      case CMD_OPTION_TYPE_SELECT:
        assert(commandLineOptions[i].variable.enumeration != NULL);
        (*commandLineOptions[i].variable.enumeration) = commandLineOptions[i].defaultValue.enumeration;
        break;
      case CMD_OPTION_TYPE_STRING:
        assert(commandLineOptions[i].variable.string != NULL);
        (*commandLineOptions[i].variable.string) = commandLineOptions[i].defaultValue.string;
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        break;
    }
  }

  endOfOptionsFlag = FALSE;
  z = 1;
  while (z < (*argc))
  {
    if      (!endOfOptionsFlag && (strcmp(argv[z],"--") == 0))
    {
      endOfOptionsFlag = TRUE;
    }
    else if (!endOfOptionsFlag && (strncmp(argv[z],"--",2) == 0))
    {
      /* get name */
      s = strchr(argv[z]+2,'=');
      if (s != NULL)
      {
        strncpy(name,argv[z]+2,MIN(s-(argv[z]+2),sizeof(name)-1));
      }
      else
      {
        strncpy(name,argv[z]+2,sizeof(name)-1);
      }
      name[sizeof(name)-1] = '\0';

      /* find option */
      i = 0;
      while ((i < commandLineOptionCount) && (strcmp(commandLineOptions[i].name,name) != 0))
      {
        i++;
      }
      if (i >= commandLineOptionCount)
      {
        if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"%sUnknown option '--%s'!\n",(errorPrefix != NULL)?errorPrefix:"",name);
        return FALSE;
      }

      /* get option value */
      if      (   (commandLineOptions[i].type == CMD_OPTION_TYPE_INTEGER)
               || (commandLineOptions[i].type == CMD_OPTION_TYPE_DOUBLE )
               || (commandLineOptions[i].type == CMD_OPTION_TYPE_SELECT )
               || (commandLineOptions[i].type == CMD_OPTION_TYPE_STRING )
               || (commandLineOptions[i].type == CMD_OPTION_TYPE_SPECIAL)
              )
      {
        if (s != NULL)
        {
          /* skip '=' */
          s++;
          value = s;
        }
        else
        {
          if ((z+1) >= (*argc))
          {
            if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"%sNo value given to option '--%s'!\n",(errorPrefix != NULL)?errorPrefix:"",name);
            return FALSE;
          }
          z++;
          value = argv[z];
        }
      }
      else if ((commandLineOptions[i].type == CMD_OPTION_TYPE_BOOLEAN))
      {
        if (s != NULL)
        {
          /* skip '=' */
          s++;
          value = s;
        }
        else
        {
          value = NULL;
        }
      }

      /* process option */
      if (!processOption(&commandLineOptions[i],"--",name,value,errorOutputHandle,errorPrefix))
      {
        return FALSE;
      }
    }
    else if (!endOfOptionsFlag && (strncmp(argv[z],"-",1) == 0))
    {
      /* get option chars */
      optionChars = argv[z]+1;
      while ((optionChars != NULL) && (*optionChars) != '\0')
      {
        /* get name */
        name[0] = (*optionChars);
        name[1] = '\0';

        /* find option */
        i = 0;
        while ((i < commandLineOptionCount) && (commandLineOptions[i].shortName != name[0]))
        {
          i++;
        }
        if (i >= commandLineOptionCount)
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"%sUnknown option '-%s'!\n",(errorPrefix != NULL)?errorPrefix:"",name);
          return FALSE;
        }

        /* find optional value for option */
        if      (   (commandLineOptions[i].type == CMD_OPTION_TYPE_INTEGER)
                 || (commandLineOptions[i].type == CMD_OPTION_TYPE_DOUBLE )
                 || (commandLineOptions[i].type == CMD_OPTION_TYPE_SELECT )
                 || (commandLineOptions[i].type == CMD_OPTION_TYPE_STRING )
                 || (commandLineOptions[i].type == CMD_OPTION_TYPE_SPECIAL)
                )
        {
          /* next argument is option value */
          if ((z+1) >= (*argc))
          {
            if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"%sNo value given to option '-%s'!\n",(errorPrefix != NULL)?errorPrefix:"",name);
            return FALSE;
          }
          z++;
          s = argv[z];
          value = s;
        }
        else
        {
          s = NULL;
          value = NULL;
        }

        /* process option */
        if (!processOption(&commandLineOptions[i],"-",name,value,errorOutputHandle,errorPrefix))
        {
          return FALSE;
        }

        /* next option char */
        if (s == NULL)
        {
          optionChars++;
        }
        else
        {
          optionChars = NULL;
        }
      }
    }
    else
    {
      /* add argument */
      argv[argumentsCount] = argv[z];
      argumentsCount++;
    }

    z++;
  }

  /* store new number of arguments */
  (*argc) = argumentsCount;

  return TRUE;
}

void cmdOptions_printHelp(FILE                    *outputHandle,
                          const CommandLineOption commandLineOptions[],
                          uint                    commandLineOptionCount
                         )
{
  #define PREFIX "Options: "

  uint i;
  uint maxNameLength;
  uint n;
  char name[128];
  char s[6];
  uint z;
  uint maxValueLength;
  uint j;

  /* get max. width of name column */
  maxNameLength = 0;
  for (i = 0; i < commandLineOptionCount; i++)
  {
    n = 0;

    if (commandLineOptions[i].shortName != '\0')
    {
      n+=3; /* "-x|" */
    }

    assert(commandLineOptions[i].name != NULL);

    /* --name */
    n+=2 + strlen(commandLineOptions[i].name);
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
      case CMD_OPTION_TYPE_DOUBLE:
        n+=4; /* =<n> */
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        n+=4; /* =0|1 */
        break;
      case CMD_OPTION_TYPE_ENUM:
        break;
      case CMD_OPTION_TYPE_SELECT:
        n+=7; /* =<name> */
        break;
      case CMD_OPTION_TYPE_STRING:
        n+=9; /* =<string> */
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        n+=6; /* =<...> */
        break;
    }

    maxNameLength = MAX(n,maxNameLength);
  }

  /* output help */
  for (i = 0; i < commandLineOptionCount; i++)
  {
    /* output prefix */
    if (i == 0)
    {
      fprintf(outputHandle,PREFIX);
    }
    else
    {
      for (z = 0; z < strlen(PREFIX); z++)
      {
        fprintf(outputHandle," ");
      }
    }

    /* output name */
    name[0] = '\0';
    if (commandLineOptions[i].shortName != '\0')
    {
      snprintf(s,sizeof(s)-1,"-%c|",commandLineOptions[i].shortName);
      strncat(name,s,sizeof(name)-strlen(name));
    }
    assert(commandLineOptions[i].name != NULL);
    strncat(name,"--",sizeof(name)-strlen(name));
    strncat(name,commandLineOptions[i].name,sizeof(name)-strlen(name));
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
      case CMD_OPTION_TYPE_DOUBLE:
        strncat(name,"=<n>",sizeof(name)-strlen(name));
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        strncat(name,"=0|1",sizeof(name)-strlen(name));
        break;
      case CMD_OPTION_TYPE_ENUM:
        break;
      case CMD_OPTION_TYPE_SELECT:
        strncat(name,"=<name>",sizeof(name)-strlen(name));
        break;
      case CMD_OPTION_TYPE_STRING:
        strncat(name,"=<string>",sizeof(name)-strlen(name));
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        strncat(name,"=<...>",sizeof(name)-strlen(name));
        break;
    }
    fprintf(outputHandle,"%s",name);

    /* output descriptions */
    for (z = strlen(name); z < maxNameLength; z++)
    {
      fprintf(outputHandle," ");
    }
    if (commandLineOptions[i].description != NULL)
    {
      fprintf(outputHandle," - %s",commandLineOptions[i].description);
    }
    fprintf(outputHandle,"\n");
    if (commandLineOptions[i].type == CMD_OPTION_TYPE_SELECT)
    {
      maxValueLength = 0;
      for (j = 0; j < commandLineOptions[i].selectCount; j++)
      {
        maxValueLength = MAX(strlen(commandLineOptions[i].selects[j].name),maxValueLength);
      }

      for (j = 0; j < commandLineOptions[i].selectCount; j++)
      {
        for (z = 0; z < strlen(PREFIX)+maxNameLength+((commandLineOptions[i].description != NULL)?3:0)+1; z++)
        {
          fprintf(outputHandle," ");
        }
        fprintf(outputHandle,"%s",commandLineOptions[i].selects[j].name);
        for (z = strlen(commandLineOptions[i].selects[j].name); z < maxValueLength; z++)
        {
          fprintf(outputHandle," ");
        }
        fprintf(outputHandle,": %s\n",commandLineOptions[i].selects[j].description);
      }
    }
  }
}

#ifdef __GNUG__
}
#endif

/* end of file */
