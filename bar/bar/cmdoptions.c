/**********************************************************************
*
* $Revision$
* $Date$
* $Author$
* Contents: command line options parser
* Systems: all
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
*          option            - option name
*          value             - option value or NULL
*          errorOutputHandle - error output handle or NULL
*          errorPrefix       - error prefix or NULL
* Output : -
* Return : TRUE if option processed without error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool processOption(const CommandLineOption *commandLineOption,
                         const char              *option,
                         const char              *value,
                         FILE                    *errorOutputHandle,
                         const char              *errorPrefix
                        )
{
  assert(commandLineOption != NULL);
  assert(option != NULL);

  switch (commandLineOption->type)
  {
    case CMD_OPTION_TYPE_INTEGER:
      {
        uint  i,n;
        char  number[128],unit[32];
        ulong factor;

        assert(commandLineOption->variable.i != NULL);

        /* split number, unit */
        i = strlen(value);
        if (i > 0)
        {
          while ((i > 0) && !isdigit(value[i-1])) { i--; }
          n = MIN(i,              sizeof(number)-1); strncpy(number,&value[0],n); number[n] = '\0';
          n = MIN(strlen(value)-i,sizeof(unit)  -1); strncpy(unit,  &value[i],n); unit[n]   = '\0';
        }
        else
        {
          number[0] = '\0';
          unit[0]   = '\0';
        }
        if (number[0] == '\0')
        {
          if (errorOutputHandle != NULL)
          {
            fprintf(errorOutputHandle,
                    "%sValue '%s' for option '%s' is not a number!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option
                   );
          }
          return FALSE;
        }

        /* find unit factor */
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
              if (errorOutputHandle != NULL)
              {
                fprintf(errorOutputHandle,
                        "%sInvalid unit in integer value '%s'! Valid units:",
                        (errorPrefix != NULL)?errorPrefix:"",
                        value
                       );
                for (i = 0; i < commandLineOption->integerOption.unitCount; i++)
                {
                  fprintf(errorOutputHandle," %s",commandLineOption->integerOption.units[i].name);
                }
                fprintf(errorOutputHandle,".\n");
              }
              return FALSE;
            }
            factor = commandLineOption->integerOption.units[i].factor;
          }
          else
          {
            if (errorOutputHandle != NULL)
            {
              fprintf(errorOutputHandle,
                      "%sUnexpected unit in value '%s'!\n",
                      (errorPrefix != NULL)?errorPrefix:"",
                      value
                     );
            }
            return FALSE;
          }
        }
        else
        {
          factor = 1;
        }

        /* calculate value */
        (*commandLineOption->variable.i) = strtol(value,NULL,0)*factor;

        /* check range */
        if (   ((*commandLineOption->variable.i) < commandLineOption->integerOption.min)
            || ((*commandLineOption->variable.i) > commandLineOption->integerOption.max)
           )
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sValue '%s' out of range %d..%d for option '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
                                                 value,
                                                 commandLineOption->integerOption.min,
                                                 commandLineOption->integerOption.max,
                                                 option
                                                );
          return FALSE;
        }
      }
      break;
    case CMD_OPTION_TYPE_INTEGER64:
      {
        uint  i,n;
        char  number[128],unit[32];
        ulong factor;

        assert(commandLineOption->variable.l != NULL);

        /* split number, unit */
        i = strlen(value);
        if (i > 0)
        {
          while ((i > 0) && !isdigit(value[i-1])) { i--; }
          n = MIN(i,              sizeof(number)-1); strncpy(number,&value[0],n); number[n] = '\0';
          n = MIN(strlen(value)-i,sizeof(unit)  -1); strncpy(unit,  &value[i],n); unit[n]   = '\0';
        }
        else
        {
          number[0] = '\0';
          unit[0]   = '\0';
        }
        if (number[0] == '\0')
        {
          if (errorOutputHandle != NULL)
          {
            fprintf(errorOutputHandle,
                    "%sValue '%s' for option '%s' is not a number!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option
                   );
          }
          return FALSE;
        }

        /* find unit factor */
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
              if (errorOutputHandle != NULL)
              {
                fprintf(errorOutputHandle,
                        "%sInvalid unit in integer value '%s'! Valid units:",
                        (errorPrefix != NULL)?errorPrefix:"",
                        value
                       );
                for (i = 0; i < commandLineOption->integer64Option.unitCount; i++)
                {
                  fprintf(errorOutputHandle," %s",commandLineOption->integer64Option.units[i].name);
                }
                fprintf(errorOutputHandle,".\n");
              }
              return FALSE;
            }
            factor = commandLineOption->integer64Option.units[i].factor;
          }
          else
          {
            if (errorOutputHandle != NULL)
            {
              fprintf(errorOutputHandle,
                      "%sUnexpected unit in value '%s'!\n",
                      (errorPrefix != NULL)?errorPrefix:"",
                      value
                     );
            }
            return FALSE;
          }
        }
        else
        {
          factor = 1;
        }

        /* calculate value */
        (*commandLineOption->variable.l) = strtoll(value,NULL,0)*factor;

        if (   ((*commandLineOption->variable.l) < commandLineOption->integer64Option.min)
            || ((*commandLineOption->variable.l) > commandLineOption->integer64Option.max)
           )
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sValue '%s' out of range %lld..%lld for option '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
                                                 value,
                                                 commandLineOption->integer64Option.min,
                                                 commandLineOption->integer64Option.max,
                                                 option
                                                );
          return FALSE;
        }
      }
      break;
    case CMD_OPTION_TYPE_DOUBLE:
      {
        uint  i,n;
        char  number[128],unit[32];
        ulong factor;

        assert(commandLineOption->variable.d != NULL);

        /* split number, unit */
        i = strlen(value);
        if (i > 0)
        {
          while ((i > 0) && !isdigit(value[i-1])) { i--; }
          n = MIN(i,              sizeof(number)-1); strncpy(number,&value[0],n); number[n] = '\0';
          n = MIN(strlen(value)-i,sizeof(unit)  -1); strncpy(unit,  &value[i],n); unit[n]   = '\0';
        }
        else
        {
          number[0] = '\0';
          unit[0]   = '\0';
        }
        if (number[0] == '\0')
        {
          if (errorOutputHandle != NULL)
          {
            fprintf(errorOutputHandle,
                    "%sValue '%s' for option '%s' is not a number!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option
                   );
          }
          return FALSE;
        }

        /* find unit factor */
        if (unit[0] != '\0')
        {
          if (commandLineOption->doubleOption.units != NULL)
          {
            i = 0;
            while ((i < commandLineOption->doubleOption.unitCount) && (strcmp(commandLineOption->doubleOption.units[i].name,unit) != 0))
            {
              i++;
            }
            if (i >= commandLineOption->doubleOption.unitCount)
            {
              if (errorOutputHandle != NULL)
              {
                fprintf(errorOutputHandle,
                        "%sInvalid unit in float value '%s'! Valid units:",
                        (errorPrefix != NULL)?errorPrefix:"",
                        value
                       );
                for (i = 0; i < commandLineOption->integerOption.unitCount; i++)
                {
                  fprintf(errorOutputHandle," %s",commandLineOption->integerOption.units[i].name);
                }
                fprintf(errorOutputHandle,".\n");
              }
              return FALSE;
            }
            factor = commandLineOption->doubleOption.units[i].factor;
          }
          else
          {
            if (errorOutputHandle != NULL)
            {
              fprintf(errorOutputHandle,
                      "%sUnexpected unit in value '%s'!\n",
                      (errorPrefix != NULL)?errorPrefix:"",
                      value
                     );
            }
            return FALSE;
          }
        }
        else
        {
          factor = 1;
        }

        (*commandLineOption->variable.d) = strtod(value,0);
        if (   ((*commandLineOption->variable.d) < commandLineOption->doubleOption.min)
            || ((*commandLineOption->variable.d) > commandLineOption->doubleOption.max)
           )
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sValue '%s' out of range %lf..%lf for float option '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
                                                 value,
                                                 commandLineOption->doubleOption.min,
                                                 commandLineOption->doubleOption.max,
                                                 option
                                                );
          return FALSE;
        }
      }
      break;
    case CMD_OPTION_TYPE_BOOLEAN:
      assert(commandLineOption->variable.b != NULL);
      if      (   (value == NULL)
               || (strcmp(value,"1") == 0)
               || (strcmp(value,"true") == 0)
               || (strcmp(value,"on") == 0)
               || (strcmp(value,"yes") == 0)
              )
      {
        (*commandLineOption->variable.b) = TRUE;
      }
      else if (   (strcmp(value,"0") == 0)
               || (strcmp(value,"false") == 0)
               || (strcmp(value,"off") == 0)
               || (strcmp(value,"no") == 0)
              )
      {
        (*commandLineOption->variable.b) = FALSE;
      }
      else
      {
        if (errorOutputHandle != NULL)
        {
          fprintf(errorOutputHandle,
                  "%sInvalid value '%s' for boolean option '%s'!\n",
                  (errorPrefix != NULL)?errorPrefix:"",
                  value,
                  option
                 );
        }
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_ENUM:
      assert(commandLineOption->variable.enumeration != NULL);
      (*commandLineOption->variable.enumeration) = commandLineOption->enumOption.enumerationValue;
      break;
    case CMD_OPTION_TYPE_SELECT:
      {
        uint z;

        assert(commandLineOption->variable.select != NULL);

        z = 0;
        while ((z < commandLineOption->selectOption.selectCount) && (strcmp(commandLineOption->selectOption.selects[z].name,value) != 0))
        {
          z++;
        }
        if (z >= commandLineOption->selectOption.selectCount)
        {
          if (errorOutputHandle != NULL)
          {
            fprintf(errorOutputHandle,
                    "%sUnknown value '%s' for option '%s'!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option
                   );
          }
          return FALSE;
        }
        (*commandLineOption->variable.select) = commandLineOption->selectOption.selects[z].value;
      }
      break;
    case CMD_OPTION_TYPE_SET:
      {
        uint  i,j,z;
        char  setName[128];

        assert(commandLineOption->variable.set != NULL);
        i = 0;
        while (value[i] != '\0')
        {
          /* skip spaces */
          while ((value[i] != '\0') && isspace(value[i])) { i++; }

          /* get name */
          j = 0;
          while ((value[i] != '\0') && (value[i] != ','))
          {
            if (j < sizeof(setName)-1) { setName[j] = value[i]; j++; }
            i++;
          }
          setName[j] = '\0';
          if (value[i] == ',') i++;

          if (setName[0] != '\0')
          {
            /* find value */
            z = 0;
            while ((z < commandLineOption->setOption.setCount) && (strcmp(commandLineOption->setOption.set[z].name,setName) != 0))
            {
              z++;
            }
            if (z >= commandLineOption->setOption.setCount)
            {
              if (errorOutputHandle != NULL)
              {
                fprintf(errorOutputHandle,
                        "%sUnknown value '%s' for option '%s'!\n",
                        (errorPrefix != NULL)?errorPrefix:"",
                        setName,
                        option
                       );
              }
              return FALSE;
            }

            /* add to set */
            (*commandLineOption->variable.set) |= commandLineOption->setOption.set[z].value;
          }
        }
      }
      break;
    case CMD_OPTION_TYPE_CSTRING:
      assert(commandLineOption->variable.cString != NULL);
      if ((*commandLineOption->variable.cString) != NULL)
      {
        free(*commandLineOption->variable.cString);
      }
      if (value != NULL)
      {
        (*commandLineOption->variable.cString) = strdup(value);
        if ((*commandLineOption->variable.cString) == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
      }
      else
      {
        (*commandLineOption->variable.cString) = NULL;
      }
      break;
    case CMD_OPTION_TYPE_STRING:
      assert(commandLineOption->variable.string != NULL);
      if (value != NULL)
      {
        if ((*commandLineOption->variable.string) != NULL)
        {
          String_setCString(*commandLineOption->variable.string,value);
        }
        else
        {
          (*commandLineOption->variable.string) = String_newCString(value);
        }
      }
      else
      {
        if ((*commandLineOption->variable.string) != NULL)
        {
          String_clear(*commandLineOption->variable.string);
        }
      }
      break;
    case CMD_OPTION_TYPE_SPECIAL:
      if (!commandLineOption->specialOption.parseSpecial(commandLineOption->specialOption.userData,
                                                         commandLineOption->variable.special,
                                                         option,
                                                         value,
                                                         commandLineOption->defaultValue.special
                                                        )
         )
      {
        if (errorOutputHandle != NULL)
        {
          fprintf(errorOutputHandle,
                  "%sInvalid value '%s' for option '%s'!\n",
                  (errorPrefix != NULL)?errorPrefix:"",
                  value,
                  option
                 );
        }
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
    (void)fwrite(SPACES8,1,8,outputHandle);
    z += 8;
  }
  while (z < n)
  {
    (void)fputc(' ',outputHandle);
    z++;
  }
}

/*---------------------------------------------------------------------*/

bool CmdOption_init(CommandLineOption commandLineOptions[],
                    uint              commandLineOptionCount
                   )
{
  uint i;
  #ifndef NDEBUG
    uint j;
  #endif /* NDEBUG */

  assert(commandLineOptions != NULL);

  #ifndef NDEBUG
    for (i = 0; i < commandLineOptionCount; i++)
    {
      for (j = 0; j < commandLineOptionCount; j++)
      {
        if (i != j)
        {
          if (strcmp(commandLineOptions[i].name,commandLineOptions[j].name) == 0)
          {
            HALT_INTERNAL_ERROR("duplicate name '%s' in command line options %d and %d",commandLineOptions[i].name,i,j);
          }
          if ((commandLineOptions[i].shortName != '\0') && (commandLineOptions[i].shortName == commandLineOptions[j].shortName))
          {
            HALT_INTERNAL_ERROR("duplicate short name '%c' in command line options %d and %d",commandLineOptions[i].shortName,i,j);
          }
        }
      }
    }
  #endif /* NDEBUG */

  for (i = 0; i < commandLineOptionCount; i++)
  {
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
        assert(commandLineOptions[i].variable.i != NULL);
        assert((*commandLineOptions[i].variable.i) >= commandLineOptions[i].integerOption.min);
        assert((*commandLineOptions[i].variable.i) <= commandLineOptions[i].integerOption.max);
        commandLineOptions[i].defaultValue.i = (*commandLineOptions[i].variable.i);
        break;
      case CMD_OPTION_TYPE_INTEGER64:
        assert(commandLineOptions[i].variable.l != NULL);
        assert((*commandLineOptions[i].variable.l) >= commandLineOptions[i].integer64Option.min);
        assert((*commandLineOptions[i].variable.l) <= commandLineOptions[i].integer64Option.max);
        commandLineOptions[i].defaultValue.l = (*commandLineOptions[i].variable.l);
        break;
      case CMD_OPTION_TYPE_DOUBLE:
        assert(commandLineOptions[i].variable.d != NULL);
        assert((*commandLineOptions[i].variable.d) >= commandLineOptions[i].doubleOption.min);
        assert((*commandLineOptions[i].variable.d) <= commandLineOptions[i].doubleOption.max);
        commandLineOptions[i].defaultValue.d = (*commandLineOptions[i].variable.d);
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        assert(commandLineOptions[i].variable.b != NULL);
        assert(   ((*commandLineOptions[i].variable.b) == TRUE )
               || ((*commandLineOptions[i].variable.b) == FALSE)
              );
        commandLineOptions[i].defaultValue.b = (*commandLineOptions[i].variable.b);
        break;
      case CMD_OPTION_TYPE_ENUM:
        assert(commandLineOptions[i].variable.enumeration != NULL);
        commandLineOptions[i].defaultValue.enumeration = (*commandLineOptions[i].variable.enumeration);
        break;
      case CMD_OPTION_TYPE_SELECT:
        assert(commandLineOptions[i].variable.select != NULL);
        commandLineOptions[i].defaultValue.select = (*commandLineOptions[i].variable.select);
        break;
      case CMD_OPTION_TYPE_SET:
        assert(commandLineOptions[i].variable.set != NULL);
        commandLineOptions[i].defaultValue.set = (*commandLineOptions[i].variable.set);
        break;
      case CMD_OPTION_TYPE_CSTRING:
        assert(commandLineOptions[i].variable.cString != NULL);
        if ((*commandLineOptions[i].variable.cString) != NULL)
        {
          commandLineOptions[i].defaultValue.cString = (*commandLineOptions[i].variable.cString);
          (*commandLineOptions[i].variable.cString) = strdup(commandLineOptions[i].defaultValue.cString);
        }
        else
        {
          commandLineOptions[i].defaultValue.cString = NULL;
        }
        break;
      case CMD_OPTION_TYPE_STRING:
        assert(commandLineOptions[i].variable.cString != NULL);
        if ((*commandLineOptions[i].variable.string) != NULL)
        {
          commandLineOptions[i].defaultValue.string = (*commandLineOptions[i].variable.string);
          (*commandLineOptions[i].variable.string) = String_duplicate(commandLineOptions[i].defaultValue.string);
        }
        else
        {
          commandLineOptions[i].defaultValue.string = NULL;
        }
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        commandLineOptions[i].defaultValue.special = commandLineOptions[i].variable.special;
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
  }

  return TRUE;
}

void CmdOption_done(CommandLineOption commandLineOptions[],
                    uint              commandLineOptionCount
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
      case CMD_OPTION_TYPE_SET:
        break;
      case CMD_OPTION_TYPE_CSTRING:
        assert(commandLineOptions[i].variable.cString != NULL);
        if ((*commandLineOptions[i].variable.cString) != NULL)
        {
          free(*commandLineOptions[i].variable.cString);
          (*commandLineOptions[i].variable.cString) = commandLineOptions[i].defaultValue.cString;
        }
        break;
      case CMD_OPTION_TYPE_STRING:
        assert(commandLineOptions[i].variable.string != NULL);
        if ((*commandLineOptions[i].variable.string) != NULL)
        {
          String_delete(*commandLineOptions[i].variable.string);
          (*commandLineOptions[i].variable.string) = commandLineOptions[i].defaultValue.string;
        }
        break;
      case CMD_OPTION_TYPE_SPECIAL:
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
  }
}

bool CmdOption_parse(const char              *argv[],
                     int                     *argc,
                     const CommandLineOption commandLineOptions[],
                     uint                    commandLineOptionCount,
                     int                     commandPriority,
                     FILE                    *errorOutputHandle,
                     const char              *errorPrefix
                    )
{
  uint       z;
  uint       minPriority,maxPriority;
  uint       priority;
  bool       endOfOptionsFlag;
  const char *s;
  char       name[128];
  char       option[128];
  uint       i;
  const char *optionChars;
  const char *value;
  int        argumentsCount;

  assert(argv != NULL);
  assert(argc != NULL);
  assert((*argc) >= 1);
  assert(commandLineOptions != NULL);

  /* get min./max. option priority */
  if (commandPriority != CMD_PRIORITY_ANY)
  {
    minPriority = commandPriority;
    maxPriority = commandPriority;
  }
  else
  {
    minPriority = 0;
    maxPriority = 0;
    for (z = 0; z < commandLineOptionCount; z++)
    {
      maxPriority = MAX(maxPriority,commandLineOptions[z].priority);
    }
  }

  /* parse options */
  argumentsCount = 1;
  for (priority = minPriority; priority <= maxPriority; priority++)
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
          value = NULL;
          if      (   (commandLineOptions[i].type == CMD_OPTION_TYPE_INTEGER  )
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_INTEGER64)
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_DOUBLE   )
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_SELECT   )
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_SET      )
                   || (commandLineOptions[i].type == CMD_OPTION_TYPE_CSTRING  )
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
                if (errorOutputHandle != NULL)
                {
                  fprintf(errorOutputHandle,
                          "%sNo value given for option '--%s'!\n",
                          (errorPrefix != NULL)?errorPrefix:"",
                          name
                         );
                }
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
          }

          if (commandLineOptions[i].priority == priority)
          {
            /* process option */
            snprintf(option,sizeof(option),"--%s",name);
            if (!processOption(&commandLineOptions[i],option,value,errorOutputHandle,errorPrefix))
            {
              return FALSE;
            }
          }
        }
        else
        {
          if (errorOutputHandle != NULL)
          {
            fprintf(errorOutputHandle,
                    "%sUnknown option '--%s'!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    name
                   );
          }
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
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_SET      )
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_CSTRING  )
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_STRING   )
                     || (commandLineOptions[i].type == CMD_OPTION_TYPE_SPECIAL  )
                    )
            {
              /* next argument is option value */
              if ((z+1) >= (*argc))
              {
                if (errorOutputHandle != NULL)
                {
                  fprintf(errorOutputHandle,
                          "%sNo value given for option '-%s'!\n",
                          (errorPrefix != NULL)?errorPrefix:"",
                          name
                         );
                }
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
              snprintf(option,sizeof(option),"-%s",name);
              if (!processOption(&commandLineOptions[i],option,value,errorOutputHandle,errorPrefix))
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
            if (errorOutputHandle != NULL)
            {
              fprintf(errorOutputHandle,
                      "%sUnknown option '-%s'!\n",
                      (errorPrefix != NULL)?errorPrefix:"",
                      name
                     );
            }
            return FALSE;
          }
        }
      }
      else
      {
        if ((commandPriority == CMD_PRIORITY_ANY) && (priority >= maxPriority))
        {
          /* add argument */
          argv[argumentsCount] = argv[z];
          argumentsCount++;
        }
      }

      z++;
    }
  }
  if (commandPriority == CMD_PRIORITY_ANY)
  {
    (*argc) = argumentsCount;
  }

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


  return processOption(commandLineOption,commandLineOption->name,value,NULL,NULL);
}

void CmdOption_printHelp(FILE                    *outputHandle,
                         const CommandLineOption commandLineOptions[],
                         uint                    commandLineOptionCount,
                         int                     helpLevel
                        )
{
  #define PREFIX "Options: "

  uint       i;
  uint       maxNameLength;
  uint       n;
  char       name[128];
  uint       j;
  char       s[6];
  uint       maxValueLength;
  const char *token;
  const char *separator;

  assert(outputHandle != NULL);
  assert(commandLineOptions != NULL);

  /* get max. width of name column */
  maxNameLength = 0;
  for (i = 0; i < commandLineOptionCount; i++)
  {
    if ((helpLevel == CMD_HELP_LEVEL_ALL) || (helpLevel >= commandLineOptions[i].helpLevel))
    {
      n = 0;

      if (commandLineOptions[i].shortName != '\0')
      {
        n += 3; /* "-x|" */
      }

      assert(commandLineOptions[i].name != NULL);

      n += 2 + strlen(commandLineOptions[i].name); /* --name */
      switch (commandLineOptions[i].type)
      {
        case CMD_OPTION_TYPE_INTEGER:
          n += 4; /* =<n> */
          if (commandLineOptions[i].integerOption.units != NULL)
          {
            n += 1; /* [ */
            for (j = 0; j < commandLineOptions[i].integerOption.unitCount; j++)
            {
              if (j > 0) n += 1; /* | */
              n += strlen(commandLineOptions[i].integerOption.units[j].name); /* unit */
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
              if (j > 0) n += 1; /* | */
              n += strlen(commandLineOptions[i].integer64Option.units[j].name); /* unit */
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
        case CMD_OPTION_TYPE_SET:
          n += 19; /* =<name>[,<name>...] */
          break;
        case CMD_OPTION_TYPE_CSTRING:
        case CMD_OPTION_TYPE_STRING:
          n += 2; /* =< */
          if (commandLineOptions[i].stringOption.helpArgument != NULL)
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
          if (commandLineOptions[i].specialOption.helpArgument != NULL)
          {
            n += strlen(commandLineOptions[i].specialOption.helpArgument);
          }
          else
          {
            n += 3; /* ... */
          }
          n += 1; /* > */
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }

      maxNameLength = MAX(n,maxNameLength);
    }
  }

  /* output help */
  for (i = 0; i < commandLineOptionCount; i++)
  {
    if ((helpLevel == CMD_HELP_LEVEL_ALL) || (helpLevel >= commandLineOptions[i].helpLevel))
    {
      /* output prefix */
      if (i == 0)
      {
        (void)fputs(PREFIX,outputHandle);
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
        case CMD_OPTION_TYPE_SET:
          strncat(name,"=<name>[,<name>...]",sizeof(name)-strlen(name));
          break;
        case CMD_OPTION_TYPE_CSTRING:
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
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      (void)fputs(name,outputHandle);

      /* output descriptions */
      assert(maxNameLength >= strlen(name));
      printSpaces(outputHandle,maxNameLength-strlen(name));
      if (commandLineOptions[i].description != NULL)
      {
        token = commandLineOptions[i].description;
        separator = strchr(token,'\n');
        if (separator != NULL)
        {
          do
          {
            (void)fputc(' ',outputHandle);
            (void)fwrite(token,1,separator-token,outputHandle);
            (void)fputc('\n',outputHandle);
            printSpaces(outputHandle,strlen(PREFIX)+maxNameLength);

            token = separator+1;
            separator = strchr(token,'\n');
          }
          while (separator != NULL);
        }
        (void)fputc(' ',outputHandle);
        (void)fputs(token,outputHandle);
      }
      switch (commandLineOptions[i].type)
      {
        case CMD_OPTION_TYPE_INTEGER:
          if (commandLineOptions[i].integerOption.rangeFlag)
          {
            if      ((commandLineOptions[i].integerOption.min > INT_MIN) && (commandLineOptions[i].integerOption.max < INT_MAX))
            {
              fprintf(outputHandle," (%d..%d",commandLineOptions[i].integerOption.min,commandLineOptions[i].integerOption.max);
            }
            else if (commandLineOptions[i].integerOption.min > INT_MIN)
            {
              fprintf(outputHandle," (>= %d",commandLineOptions[i].integerOption.min);
            }
            else if (commandLineOptions[i].integerOption.max < INT_MAX)
            {
              fprintf(outputHandle," (<= %d",commandLineOptions[i].integerOption.max);
            }
            if (commandLineOptions[i].defaultValue.i != 0)
            {
              fprintf(outputHandle,", default: %d",commandLineOptions[i].defaultValue.i);
            }
            (void)fputc(')',outputHandle);
          }
          else
          {
            if (commandLineOptions[i].defaultValue.i != 0)
            {
              fprintf(outputHandle," (default: %d)",commandLineOptions[i].defaultValue.i);
            }
          }
          break;
        case CMD_OPTION_TYPE_INTEGER64:
          if (commandLineOptions[i].integer64Option.rangeFlag)
          {
            if      ((commandLineOptions[i].integer64Option.min > INT_MIN) && (commandLineOptions[i].integer64Option.max < INT_MAX))
            {
              fprintf(outputHandle," (%lld..%lld",commandLineOptions[i].integer64Option.min,commandLineOptions[i].integer64Option.max);
            }
            else if (commandLineOptions[i].integer64Option.min > INT_MIN)
            {
              fprintf(outputHandle," (>= %lld",commandLineOptions[i].integer64Option.min);
            }
            else if (commandLineOptions[i].integer64Option.max < INT_MAX)
            {
              fprintf(outputHandle," (<= %lld",commandLineOptions[i].integer64Option.max);
            }
            if (commandLineOptions[i].defaultValue.l != 0LL)
            {
              fprintf(outputHandle,", default: %lld",commandLineOptions[i].defaultValue.l);
            }
            (void)fputc(')',outputHandle);
          }
          else
          {
            if (commandLineOptions[i].defaultValue.l != 0LL)
            {
              fprintf(outputHandle," (default: %lld)",commandLineOptions[i].defaultValue.l);
            }
          }
          break;
        case CMD_OPTION_TYPE_DOUBLE:
          if (commandLineOptions[i].doubleOption.rangeFlag)
          {
            if      ((commandLineOptions[i].doubleOption.min > DBL_MIN) && (commandLineOptions[i].doubleOption.max < DBL_MAX))
            {
              fprintf(outputHandle," (%lf..%lf",commandLineOptions[i].doubleOption.min,commandLineOptions[i].doubleOption.max);
            }
            else if (commandLineOptions[i].doubleOption.min > DBL_MIN)
            {
              fprintf(outputHandle," (>= %lf",commandLineOptions[i].doubleOption.min);
            }
            else if (commandLineOptions[i].doubleOption.max < DBL_MAX)
            {
              fprintf(outputHandle," (<= %lf",commandLineOptions[i].doubleOption.max);
            }
            if (commandLineOptions[i].defaultValue.d != 0.0)
            {
              fprintf(outputHandle,", default: %lf",commandLineOptions[i].defaultValue.d);
            }
            (void)fputc(')',outputHandle);
          }
          else
          {
            if (commandLineOptions[i].defaultValue.d != 0.0)
            {
              fprintf(outputHandle," (default: %lf)",commandLineOptions[i].defaultValue.d);
            }
          }
          break;
        case CMD_OPTION_TYPE_BOOLEAN:
          break;
        case CMD_OPTION_TYPE_ENUM:
          break;
        case CMD_OPTION_TYPE_SELECT:
          break;
        case CMD_OPTION_TYPE_SET:
          break;
        case CMD_OPTION_TYPE_CSTRING:
          if (commandLineOptions[i].defaultValue.cString != NULL)
          {
            fprintf(outputHandle," (default: %s)",commandLineOptions[i].defaultValue.cString);
          }
          break;
        case CMD_OPTION_TYPE_STRING:
          if (commandLineOptions[i].defaultValue.cString != NULL)
          {
            fprintf(outputHandle," (default: %s)",String_cString(commandLineOptions[i].defaultValue.string));
          }
          break;
        case CMD_OPTION_TYPE_SPECIAL:
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      (void)fputc('\n',outputHandle);
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
            printSpaces(outputHandle,strlen(PREFIX)+maxNameLength+((commandLineOptions[i].description != NULL)?2:0)+1);
            (void)fputs(commandLineOptions[i].selectOption.selects[j].name,outputHandle);
            printSpaces(outputHandle,maxValueLength-strlen(commandLineOptions[i].selectOption.selects[j].name));
            (void)fputs(": ",outputHandle);
            (void)fputs(commandLineOptions[i].selectOption.selects[j].description,outputHandle);
            if (commandLineOptions[i].selectOption.selects[j].value == commandLineOptions[i].defaultValue.select)
            {
              (void)fputs(" (default)",outputHandle);
            }
            (void)fputc('\n',outputHandle);
          }
          break;
        case CMD_OPTION_TYPE_SET:
          maxValueLength = 0;
          for (j = 0; j < commandLineOptions[i].setOption.setCount; j++)
          {
            maxValueLength = MAX(strlen(commandLineOptions[i].setOption.set[j].name),maxValueLength);
          }

          for (j = 0; j < commandLineOptions[i].setOption.setCount; j++)
          {
            printSpaces(outputHandle,strlen(PREFIX)+maxNameLength+((commandLineOptions[i].description != NULL)?2:0)+1);
            (void)fputs(commandLineOptions[i].setOption.set[j].name,outputHandle);
            printSpaces(outputHandle,maxValueLength-strlen(commandLineOptions[i].setOption.set[j].name));
            (void)fputs(": ",outputHandle);
            (void)fputs(commandLineOptions[i].setOption.set[j].description,outputHandle);
            if (commandLineOptions[i].setOption.set[j].value == commandLineOptions[i].defaultValue.set)
            {
              (void)fputs(" (default)",outputHandle);
            }
            (void)fputc('\n',outputHandle);
          }
          break;
        case CMD_OPTION_TYPE_CSTRING:
          break;
        case CMD_OPTION_TYPE_STRING:
          break;
        case CMD_OPTION_TYPE_SPECIAL:
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
    }
  }
}

#ifdef __GNUG__
}
#endif

/* end of file */
