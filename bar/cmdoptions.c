/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/cmdoptions.c,v $
* $Revision: 1.8 $
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
  uint  i,n;
  char  number[64],unit[32];
  ulong factor;

  assert(commandLineOption != NULL);
  assert(prefix != NULL);
  assert(name != NULL);

  switch (commandLineOption->type)
  {
    case CMD_OPTION_TYPE_INTEGER:
      assert(commandLineOption->variable.n != NULL);

      /* split number, unit */
      i = strlen(value);
      if (i > 0)
      {
        while ((i > 1) && !isdigit(value[i-1])) { i--; }
        n = MIN(i,              sizeof(number)-1); strncpy(number,&value[0],n);number[n] = '\0';
        n = MIN(strlen(value)-i,sizeof(unit)  -1); strncpy(unit,  &value[i],n);unit[n]   = '\0';
      }
      else
      {
        number[0] = '\0';
        unit[0]   = '\0';
      }

      /* find factor */
      if (unit[0] != '\0')
      {
        if (commandLineOption->integerOption.units != NULL)
        {
          i = 0;
          while ((i < commandLineOption->integerOption.unitCount) && (strcmp(commandLineOption->integerOption.units[i].name,unit) != 0))
          {
            i++;
          }
          if (i >= commandLineOption->integerOption.unitCount)
          {
            if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"Invalid unit in integer value '%s'!\n",value);
            return FALSE;
          }
          factor = commandLineOption->integerOption.units[i].factor;
        }
        else
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"Unexpected unit in value '%s'!\n",value);
          return FALSE;
        }
      }
      else
      {
        factor = 1;
      }

      (*commandLineOption->variable.n) = strtoll(value,NULL,0)*factor;
      if (   ((*commandLineOption->variable.n) < commandLineOption->integerOption.min)
          || ((*commandLineOption->variable.n) > commandLineOption->integerOption.max)
         )
      {
        if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                               "Value '%s' out range %d..%d for option '%s%s'!\n",
                                               value,
                                               commandLineOption->integerOption.min,
                                               commandLineOption->integerOption.max,
                                               prefix,
                                               name
                                              );
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_INTEGER64:
      assert(commandLineOption->variable.l != NULL);

      /* split number, unit */
      i = strlen(value);
      if (i > 0)
      {
        while ((i > 1) && !isdigit(value[i-1])) { i--; }
        n = MIN(i,              sizeof(number)-1); strncpy(number,&value[0],n);number[n] = '\0';
        n = MIN(strlen(value)-i,sizeof(unit)  -1); strncpy(unit,  &value[i],n);unit[n]   = '\0';
      }
      else
      {
        number[0] = '\0';
        unit[0]   = '\0';
      }

      /* find factor */
      if (unit[0] != '\0')
      {
        if (commandLineOption->integer64Option.units != NULL)
        {
          i = 0;
          while ((i < commandLineOption->integer64Option.unitCount) && (strcmp(commandLineOption->integer64Option.units[i].name,unit) != 0))
          {
            i++;
          }
          if (i >= commandLineOption->integer64Option.unitCount)
          {
            if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"Invalid unit in integer value '%s'!\n",value);
            return FALSE;
          }
          factor = commandLineOption->integer64Option.units[i].factor;
        }
        else
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"Unexpected unit in value '%s'!\n",value);
          return FALSE;
        }
      }
      else
      {
        factor = 1;
      }

      (*commandLineOption->variable.n) = strtoll(value,NULL,0)*factor;
      if (   ((*commandLineOption->variable.n) < commandLineOption->integer64Option.min)
          || ((*commandLineOption->variable.n) > commandLineOption->integer64Option.max)
         )
      {
        if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                               "Value '%s' out range %lld..%lld for option '%s%s'!\n",
                                               value,
                                               commandLineOption->integer64Option.min,
                                               commandLineOption->integer64Option.max,
                                               prefix,
                                               name
                                              );
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_DOUBLE:
      assert(commandLineOption->variable.d != NULL);
      (*commandLineOption->variable.d) = strtod(value,0);
      if (   ((*commandLineOption->variable.d) < commandLineOption->doubleOption.min)
          || ((*commandLineOption->variable.d) > commandLineOption->doubleOption.max)
         )
      {
        if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                               "Value '%s' out range %lf..%lf for float option '%s%s'!\n",
                                               value,
                                               commandLineOption->doubleOption.min,
                                               commandLineOption->doubleOption.max,
                                               prefix,
                                               name
                                              );
        return FALSE;
      }
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
      (*commandLineOption->variable.enumeration) = commandLineOption->enumOption.enumerationValue;
      break;
    case CMD_OPTION_TYPE_SELECT:
      assert(commandLineOption->variable.select != NULL);
      i = 0;
      while ((i < commandLineOption->selectOption.selectCount) && (strcmp(commandLineOption->selectOption.selects[i].name,value) != 0))
      {
        i++;
      }
      if (i >= commandLineOption->selectOption.selectCount)
      {
        if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"Unknown value '%s' for option '%s%s'!\n",value,prefix,name);
        return FALSE;
      }
      (*commandLineOption->variable.enumeration) = commandLineOption->selectOption.selects[i].value;
      break;
    case CMD_OPTION_TYPE_STRING:
      assert(commandLineOption->variable.string != NULL);
      if (   ((*commandLineOption->variable.string) != NULL)
          && ((*commandLineOption->variable.string) != commandLineOption->defaultValue.string)
         )
      {
        free(*commandLineOption->variable.string);
      }
      (*commandLineOption->variable.string) = strdup(value);
      break;

    case CMD_OPTION_TYPE_SPECIAL:
      if (!commandLineOption->specialOption.parseSpecial(commandLineOption->variable.special,
                                                         commandLineOption->name,
                                                         value,
                                                         commandLineOption->defaultValue.p,
                                                         commandLineOption->specialOption.userData
                                                        )
         )
      {
        return FALSE;
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : printSpaces
* Purpose: print spaces
* Input  : outputHandle - output file handle
*          n            - number of spaces to print
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printSpaces(FILE *outputHandle, uint n)
{
  const char *SPACES8 = "        ";

  uint z;

  assert(outputHandle != NULL);

  z = 0;
  while ((z+8) < n)
  {
    fprintf(outputHandle,SPACES8);
    z += 8;
  }
  while (z < n)
  {
    fprintf(outputHandle," ");
    z++;
  }
}

/*---------------------------------------------------------------------*/

bool CmdOption_init(const CommandLineOption commandLineOptions[],
                    uint                    commandLineOptionCount
                   )
{
  uint i;

  assert(commandLineOptions != NULL);

  for (i = 0; i < commandLineOptionCount; i++)
  {
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
        assert(commandLineOptions[i].variable.n != NULL);
        (*commandLineOptions[i].variable.n) = commandLineOptions[i].defaultValue.n;
        break;
      case CMD_OPTION_TYPE_INTEGER64:
        assert(commandLineOptions[i].variable.l != NULL);
        (*commandLineOptions[i].variable.l) = commandLineOptions[i].defaultValue.l;
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
        assert(commandLineOptions[i].variable.select != NULL);
        (*commandLineOptions[i].variable.select) = commandLineOptions[i].defaultValue.select;
        break;
      case CMD_OPTION_TYPE_STRING:
        assert(commandLineOptions[i].variable.string != NULL);
        (*commandLineOptions[i].variable.string) = (char*)commandLineOptions[i].defaultValue.string;
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        if (commandLineOptions[i].defaultValue.p != NULL)
        {
          if (!commandLineOptions[i].specialOption.parseSpecial(commandLineOptions[i].variable.special,
                                                                commandLineOptions[i].name,
                                                                commandLineOptions[i].defaultValue.p,
                                                                NULL,
                                                                commandLineOptions[i].specialOption.userData
                                                               )
             )
          {
            return FALSE;
          }
        }
        break;
    }
  }

  return TRUE;
}

void CmdOption_done(const CommandLineOption commandLineOptions[],
                    uint                    commandLineOptionCount
                   )
{
  uint i;

  assert(commandLineOptions != NULL);

  for (i = 0; i < commandLineOptionCount; i++)
  {
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
        break;
      case CMD_OPTION_TYPE_INTEGER64:
        break;
      case CMD_OPTION_TYPE_DOUBLE:
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        break;
      case CMD_OPTION_TYPE_ENUM:
        break;
      case CMD_OPTION_TYPE_SELECT:
        break;
      case CMD_OPTION_TYPE_STRING:
        assert(commandLineOptions[i].variable.string != NULL);
        if (   ((*commandLineOptions[i].variable.string) != NULL)
            && ((*commandLineOptions[i].variable.string) != commandLineOptions[i].defaultValue.string)
           )
        {
          free(*commandLineOptions[i].variable.string);
        }
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        break;
    }
  }
}

bool CmdOption_parse(const char              *argv[],
                     int                     *argc,
                     const CommandLineOption commandLineOptions[],
                     uint                    commandLineOptionCount,
                     FILE                    *errorOutputHandle,
                     const char              *errorPrefix
                    )
{
  uint         i;
  unsigned int priority,maxPriority;
  bool         endOfOptionsFlag;
  uint         z;
  const char   *s;
  char         name[128];
  const char   *optionChars;
  const char   *value;
  int          argumentsCount;

  assert(argv != NULL);
  assert(argc != NULL);
  assert((*argc) >= 1);
  assert(commandLineOptions != NULL);

  /* get max. option priority */
  maxPriority = 0;
  for (i = 0; i < commandLineOptionCount; i++)
  {
    maxPriority = MAX(maxPriority,commandLineOptions[i].priority);
  }

  /* parse options */
  argumentsCount = 1;
  for (priority = 0; priority <= maxPriority; priority++)
  {
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
          name[MIN(s-(argv[z]+2),sizeof(name)-1)] = '\0';
        }
        else
        {
          strncpy(name,argv[z]+2,sizeof(name)-1);
          name[sizeof(name)-1] = '\0';
        }

        /* find option */
        i = 0;
        while ((i < commandLineOptionCount) && (strcmp(commandLineOptions[i].name,name) != 0))
        {
          i++;
        }
        if (i < commandLineOptionCount)
        {
          /* get option value */
          if      (   (commandLineOptions[i].type == CMD_OPTION_TYPE_INTEGER  )
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_INTEGER64)
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_DOUBLE   )
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_SELECT   )
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_STRING   )
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_SPECIAL  )
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
                if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"%sNo value given for option '--%s'!\n",(errorPrefix != NULL)?errorPrefix:"",name);
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

          if (commandLineOptions[i].priority == priority)
          {
            /* process option */
            if (!processOption(&commandLineOptions[i],"--",name,value,errorOutputHandle,errorPrefix))
            {
              return FALSE;
            }
          }
        }
        else
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"%sUnknown option '--%s'!\n",(errorPrefix != NULL)?errorPrefix:"",name);
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
          if (i < commandLineOptionCount)
          {
            /* find optional value for option */
            if      (   (commandLineOptions[i].type == CMD_OPTION_TYPE_INTEGER  )
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_INTEGER64)
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_DOUBLE   )
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_SELECT   )
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_STRING   )
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_SPECIAL  )
                    )
            {
              /* next argument is option value */
              if ((z+1) >= (*argc))
              {
                if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"%sNo value given for option '-%s'!\n",(errorPrefix != NULL)?errorPrefix:"",name);
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

            if (commandLineOptions[i].priority == priority)
            {
              /* process option */
              if (!processOption(&commandLineOptions[i],"-",name,value,errorOutputHandle,errorPrefix))
              {
                return FALSE;
              }
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
          else
          {
            if (errorOutputHandle != NULL) fprintf(errorOutputHandle,"%sUnknown option '-%s'!\n",(errorPrefix != NULL)?errorPrefix:"",name);
            return FALSE;
          }
        }
      }
      else
      {
        if (priority >= maxPriority)
        {
          /* add argument */
          argv[argumentsCount] = argv[z];
          argumentsCount++;
        }
      }

      z++;
    }
  }
  (*argc) = argumentsCount;

  return TRUE;
}

const CommandLineOption *CmdOption_find(const char              *name,
                                        const CommandLineOption commandLineOptions[],
                                        uint                    commandLineOptionCount
                                       )
{
  uint i;

  assert(commandLineOptions != NULL);

  i = 0;
  while ((i < commandLineOptionCount) && (strcmp(commandLineOptions[i].name,name) != 0))
  {
    i++;
  }

  return (i < commandLineOptionCount)?&commandLineOptions[i]:NULL;
}

bool CmdOption_parseString(const CommandLineOption *commandLineOption,
                           const char              *value
                          )
{
  assert(commandLineOption != NULL);


  return processOption(commandLineOption,"",commandLineOption->name,value,NULL,NULL);
}

void CmdOption_printHelp(FILE                    *outputHandle,
                         const CommandLineOption commandLineOptions[],
                         uint                    commandLineOptionCount
                        )
{
  #define PREFIX "Options: "

  uint i;
  uint maxNameLength;
  uint n;
  char name[128];
  uint j;
  char s[6];
  uint maxValueLength;

  assert(outputHandle != NULL);
  assert(commandLineOptions != NULL);

  /* get max. width of name column */
  maxNameLength = 0;
  for (i = 0; i < commandLineOptionCount; i++)
  {
    n = 0;

    if (commandLineOptions[i].shortName != '\0')
    {
      n += 3; /* "-x|" */
    }

    assert(commandLineOptions[i].name != NULL);

    /* --name */
    n += 2 + strlen(commandLineOptions[i].name);
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
        n += 4; /* =<n> */
        if (commandLineOptions[i].integerOption.units != NULL)
        {
          n += 1; /* [ */
          for (j = 0; j < commandLineOptions[i].integerOption.unitCount; j++)
          {
            if (j > 0) n += 1;
            n += strlen(commandLineOptions[i].integerOption.units[j].name);
          }       
          n += 1; /* ] */
        }
        break;
      case CMD_OPTION_TYPE_INTEGER64:
        n += 4; /* =<n> */
        if (commandLineOptions[i].integer64Option.units != NULL)
        {
          n += 1; /* [ */
          for (j = 0; j < commandLineOptions[i].integer64Option.unitCount; j++)
          {
            if (j > 0) n += 1;
            n += strlen(commandLineOptions[i].integer64Option.units[j].name);
          }       
          n += 1; /* ] */
        }
        break;
      case CMD_OPTION_TYPE_DOUBLE:
        n += 4; /* =<n> */
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        if (commandLineOptions[i].booleanOption.yesnoFlag) 
        {
          n += 9; /* [=yes|no] */
        }
        break;
      case CMD_OPTION_TYPE_ENUM:
        break;
      case CMD_OPTION_TYPE_SELECT:
        n += 7; /* =<name> */
        break;
      case CMD_OPTION_TYPE_STRING:
        n += 2; /* =< */
        if (commandLineOptions[i].specialOption.helpArgument != NULL)
        {
          n += strlen(commandLineOptions[i].stringOption.helpArgument);
        }
        else
        {
          n += 6; /* string */
        }
        n += 1; /* > */
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        n += 2; /* =< */
        strncat(name,"=<",sizeof(name)-strlen(name));
        if (commandLineOptions[i].specialOption.helpArgument != NULL)
        {
          n += strlen(commandLineOptions[i].specialOption.helpArgument);
        }
        else
        {
          n += 2; /* ... */
        }
        n += 1; /* > */
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
      printSpaces(outputHandle,strlen(PREFIX));
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
        strncat(name,"=<n>",sizeof(name)-strlen(name));
        if (commandLineOptions[i].integerOption.units != NULL)
        {
          strncat(name,"[",sizeof(name)-strlen(name));
          for (j = 0; j < commandLineOptions[i].integerOption.unitCount; j++)
          {
            if (j > 0) strncat(name,"|",sizeof(name)-strlen(name));
            strncat(name,commandLineOptions[i].integerOption.units[j].name,sizeof(name)-strlen(name));
          }       
          strncat(name,"]",sizeof(name)-strlen(name));
        }
        break;
      case CMD_OPTION_TYPE_INTEGER64:
        strncat(name,"=<n>",sizeof(name)-strlen(name));
        if (commandLineOptions[i].integer64Option.units != NULL)
        {
          strncat(name,"[",sizeof(name)-strlen(name));
          for (j = 0; j < commandLineOptions[i].integer64Option.unitCount; j++)
          {
            if (j > 0) strncat(name,"|",sizeof(name)-strlen(name));
            strncat(name,commandLineOptions[i].integer64Option.units[j].name,sizeof(name)-strlen(name));
          }       
          strncat(name,"]",sizeof(name)-strlen(name));
        }
        break;
      case CMD_OPTION_TYPE_DOUBLE:
        strncat(name,"=<n>",sizeof(name)-strlen(name));
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        if (commandLineOptions[i].booleanOption.yesnoFlag) 
        {
          strncat(name,"[=yes|no]",sizeof(name)-strlen(name));
        }
        break;
      case CMD_OPTION_TYPE_ENUM:
        break;
      case CMD_OPTION_TYPE_SELECT:
        strncat(name,"=<name>",sizeof(name)-strlen(name));
        break;
      case CMD_OPTION_TYPE_STRING:
        strncat(name,"=<",sizeof(name)-strlen(name));
        if (commandLineOptions[i].stringOption.helpArgument != NULL)
        {
          strncat(name,commandLineOptions[i].stringOption.helpArgument,sizeof(name)-strlen(name));
        }
        else
        {
          strncat(name,"string",sizeof(name)-strlen(name));
        }
        strncat(name,">",sizeof(name)-strlen(name));
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        strncat(name,"=<",sizeof(name)-strlen(name));
        if (commandLineOptions[i].specialOption.helpArgument != NULL)
        {
          strncat(name,commandLineOptions[i].specialOption.helpArgument,sizeof(name)-strlen(name));
        }
        else
        {
          strncat(name,"...",sizeof(name)-strlen(name));
        }
        strncat(name,">",sizeof(name)-strlen(name));
        break;
    }
    fprintf(outputHandle,"%s",name);

    /* output descriptions */
    printSpaces(outputHandle,maxNameLength-strlen(name));
    if (commandLineOptions[i].description != NULL)
    {
      fprintf(outputHandle," - %s",commandLineOptions[i].description);
    }
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
        if (commandLineOptions[i].integerOption.rangeFlag)
        {
          if      ((commandLineOptions[i].integerOption.min > INT_MIN) && (commandLineOptions[i].integerOption.max < INT_MAX))
          {
            fprintf(outputHandle," (%d..%d)",commandLineOptions[i].integerOption.min,commandLineOptions[i].integerOption.max);
          }
          else if (commandLineOptions[i].integerOption.min > INT_MIN)
          {
            fprintf(outputHandle," (>= %d)",commandLineOptions[i].integerOption.min);
          }
          else if (commandLineOptions[i].integerOption.max < INT_MAX)
          {
            fprintf(outputHandle," (<= %d)",commandLineOptions[i].integerOption.max);
          }
        }
        break;
      case CMD_OPTION_TYPE_INTEGER64:
        if (commandLineOptions[i].integer64Option.rangeFlag)
        {
          if      ((commandLineOptions[i].integer64Option.min > INT_MIN) && (commandLineOptions[i].integer64Option.max < INT_MAX))
          {
            fprintf(outputHandle," (%lld..%lld)",commandLineOptions[i].integer64Option.min,commandLineOptions[i].integer64Option.max);
          }
          else if (commandLineOptions[i].integer64Option.min > INT_MIN)
          {
            fprintf(outputHandle," (>= %lld)",commandLineOptions[i].integer64Option.min);
          }
          else if (commandLineOptions[i].integer64Option.max < INT_MAX)
          {
            fprintf(outputHandle," (<= %lld)",commandLineOptions[i].integer64Option.max);
          }
        }
        break;
      case CMD_OPTION_TYPE_DOUBLE:
        if (commandLineOptions[i].doubleOption.rangeFlag)
        {
          if      ((commandLineOptions[i].doubleOption.min > DBL_MIN) && (commandLineOptions[i].doubleOption.max < DBL_MAX))
          {
            fprintf(outputHandle," (%lf..%lf)",commandLineOptions[i].doubleOption.min,commandLineOptions[i].doubleOption.max);
          }
          else if (commandLineOptions[i].doubleOption.min > DBL_MIN)
          {
            fprintf(outputHandle," (>= %lf)",commandLineOptions[i].doubleOption.min);
          }
          else if (commandLineOptions[i].doubleOption.max < DBL_MAX)
          {
            fprintf(outputHandle," (<= %lf)",commandLineOptions[i].doubleOption.max);
          }
        }
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        break;
      case CMD_OPTION_TYPE_ENUM:
        break;
      case CMD_OPTION_TYPE_SELECT:
        break;
      case CMD_OPTION_TYPE_STRING:
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        break;
    }
    fprintf(outputHandle,"\n");
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
      case CMD_OPTION_TYPE_INTEGER64:
        break;
      case CMD_OPTION_TYPE_DOUBLE:
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        break;
      case CMD_OPTION_TYPE_ENUM:
        break;
      case CMD_OPTION_TYPE_SELECT:
        maxValueLength = 0;
        for (j = 0; j < commandLineOptions[i].selectOption.selectCount; j++)
        {
          maxValueLength = MAX(strlen(commandLineOptions[i].selectOption.selects[j].name),maxValueLength);
        }

        for (j = 0; j < commandLineOptions[i].selectOption.selectCount; j++)
        {
          printSpaces(outputHandle,strlen(PREFIX)+maxNameLength+((commandLineOptions[i].description != NULL)?3:0)+1);
          fprintf(outputHandle,"%s",commandLineOptions[i].selectOption.selects[j].name);
          printSpaces(outputHandle,maxValueLength-strlen(commandLineOptions[i].selectOption.selects[j].name));
          fprintf(outputHandle,": %s",commandLineOptions[i].selectOption.selects[j].description);
          if (commandLineOptions[i].selectOption.selects[j].value == commandLineOptions[i].defaultValue.select)
          {
            fprintf(outputHandle," (default)");
          }
          fprintf(outputHandle,"\n");
        }
        break;
      case CMD_OPTION_TYPE_STRING:
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        break;
    }
  }
}

#ifdef __GNUG__
}
#endif

/* end of file */
