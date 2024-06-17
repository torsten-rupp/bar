/**********************************************************************
*
* Contents: command line options parser
* Systems: all
*
***********************************************************************/

#define __CMDOPTION_IMPLEMENTATION__

/****************************** Includes ******************************/
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "common/global.h"
#include "common/arrays.h"

#include "cmdoptions.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/

/***************************** Datatypes ******************************/

/***************************** Variables ******************************/
Array cmdSetOptions;

/******************************* Macros *******************************/
#define ITERATE_UNITS(unit,units) \
  for ((unit) = units; \
       (unit)->name != NULL; \
       (unit)++ \
      )

#define ITERATE_SELECT(select,selects) \
  for ((select) = selects; \
       (select)->name != NULL; \
       (select)++ \
      )

#define ITERATE_SET(set,sets) \
  for ((set) = sets; \
       (set)->name != NULL; \
       (set)++ \
      )

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************\
* Name   : findUnit
* Purpose: find unit by name
* Input  : units    - units array
*          unitName - unit name
* Output : -
* Return : unit or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const CommandLineUnit *findUnit(const CommandLineUnit *units, const char *unitName)
{
  const CommandLineUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && !stringEquals(unit->name,unitName)
          )
    {
      unit++;
    }
    return (unit->name != NULL) ? unit : NULL;
  }
  else
  {
    return NULL;
  }
}

/***********************************************************************\
* Name   : findIntegerUnitByValue
* Purpose: find matching unit by value
* Input  : units - units array
*          value - value
* Output : -
* Return : unit or NULL if no matching unit found
* Notes  : -
\***********************************************************************/

LOCAL const CommandLineUnit *findIntegerUnitByValue(const CommandLineUnit *units, int value)
{
  const CommandLineUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && ((value % unit->factor) != 0)
          )
    {
      unit++;
    }
    return (unit->name != NULL) ? unit : NULL;
  }
  else
  {
    return NULL;
  }
}

/***********************************************************************\
* Name   : findInteger64UnitByValue
* Purpose: find unit by value
* Input  : units - units array
*          value - value
* Output : -
* Return : unit or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const CommandLineUnit *findInteger64UnitByValue(const CommandLineUnit *units, int64 value)
{
  const CommandLineUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && ((value % unit->factor) != 0)
          )
    {
      unit++;
    }
    return (unit->name != NULL) ? unit : NULL;
  }
  else
  {
    return NULL;
  }
}

#if 0
// still not used
/***********************************************************************\
* Name   : findDoubleUnitByValue
* Purpose: find unit by name
* Input  : units - units array
*          value - value
* Output : -
* Return : unit or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const CommandLineUnit *findDoubleUnitByValue(const CommandLineUnit *units, double value)
{
  const CommandLineUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && (fmod(value,units->factor) != 0.0)
          )
    {
      unit++;
    }
    return (unit->name != NULL) ? unit : NULL;
  }
  else
  {
    return NULL;
  }
}
#endif

/***********************************************************************\
* Name   : findSelect
* Purpose: find select by name
* Input  : selects    - selects array
*          selectName - select name
* Output : -
* Return : select or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const CommandLineOptionSelect *findSelect(const CommandLineOptionSelect *selects, const char *selectName)
{
  const CommandLineOptionSelect *select;

  if (selects != NULL)
  {
    select = selects;
    while (   (select->name != NULL)
           && !stringEqualsIgnoreCase(select->name,selectName)
          )
    {
      select++;
    }
    return (select->name != NULL) ? select : NULL;
  }
  else
  {
    return NULL;
  }
}

#if 0
// still not used
/***********************************************************************\
* Name   : findSelectByValue
* Purpose: find select by value
* Input  : selects - selects array
*          value   - value
* Output : -
* Return : select or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const CommandLineOptionSelect *findSelectByValue(const CommandLineOptionSelect *selects, uint value)
{
  const CommandLineOptionSelect *select;

  if (selects != NULL)
  {
    select = selects;
    while (   (select->name != NULL)
           && (select->value != value)
          )
    {
      select++;
    }
    return (select->name != NULL) ? select : NULL;
  }
  else
  {
    return NULL;
  }
}
#endif

/***********************************************************************\
* Name   : findSet
* Purpose: find set by name
* Input  : sets    - sets array
*          setName - set name
* Output : -
* Return : set or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const CommandLineOptionSet *findSet(const CommandLineOptionSet *sets, const char *setName)
{
  const CommandLineOptionSet *set;

  if (sets != NULL)
  {
    set = sets;
    while (   (set->name != NULL)
           && !stringEqualsIgnoreCase(set->name,setName)
          )
    {
      set++;
    }
    return (set->name != NULL) ? set : NULL;
  }
  else
  {
    return NULL;
  }
}

#if 0
// still not used
/***********************************************************************\
* Name   : findSetByValue
* Purpose: find set by value
* Input  : sets  - sets array
*          value - value
* Output : -
* Return : set or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const CommandLineOptionSet *findSetByValue(const CommandLineOptionSet *sets, uint value)
{
  const CommandLineOptionSet *set;

  if (sets != NULL)
  {
    set = sets;
    while (   (set->name != NULL)
           && (set->value != value)
          )
    {
      set++;
    }
    return (set->name != NULL) ? set : NULL;
  }
  else
  {
    return NULL;
  }
}
#endif

/***********************************************************************\
* Name   : getIntegerOption
* Purpose: get integer option value
* Input  : value         - value variable
*          string        - string
*          name          - option name
*          units         - units array or NULL
*          outputHandle  - output handle
*          errorPrefix   - error prefix or NULL
*          warningPrefix - warning prefix or NULL
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getIntegerOption(int                   *value,
                            const char            *string,
                            const char            *name,
                            const CommandLineUnit *units,
                            FILE                  *outputHandle,
                            const char            *errorPrefix,
                            const char            *warningPrefix
                           )
{
  uint                  i,j;
  char                  number[128],unitName[32];
  const CommandLineUnit *unit;
  ulong                 factor;

  assert(value != NULL);
  assert(string != NULL);

  UNUSED_VARIABLE(warningPrefix);

  // split number, unit
  i = strlen(string);
  if (i > 0)
  {
    while ((i > 0) && !isdigit(string[i-1])) { i--; }
//TODO: stringSub
    j = MIN(i,               sizeof(number  )-1); strncpy(number,  &string[0],j); number  [j] = '\0';
    j = MIN(strlen(string)-i,sizeof(unitName)-1); strncpy(unitName,&string[i],j); unitName[j] = '\0';
  }
  else
  {
    number  [0] = '\0';
    unitName[0] = '\0';
  }
  if (number[0] == '\0')
  {
    if (outputHandle != NULL)
    {
      fprintf(outputHandle,
              "%sValue '%s' for option '%s' is not a number!\n",
              (errorPrefix != NULL) ? errorPrefix : "",
              string,
              name
             );
    }
    return FALSE;
  }

  // find unit factor
  if (unitName[0] != '\0')
  {
    if (units != NULL)
    {
      unit = findUnit(units,unitName);
      if (unit == NULL)
      {
        if (outputHandle != NULL)
        {
          fprintf(outputHandle,
                  "%sInvalid unit in integer value '%s'! Valid units:",
                  (errorPrefix != NULL) ? errorPrefix : "",
                  string
                 );
          ITERATE_UNITS(unit,units)
          {
            fprintf(outputHandle," %s",unit->name);
          }
          fprintf(outputHandle,".\n");
        }
        return FALSE;
      }
      factor = unit->factor;
    }
    else
    {
      if (outputHandle != NULL)
      {
        fprintf(outputHandle,
                "%sUnexpected unit '%s' in value '%s'!\n",
                (errorPrefix != NULL) ? errorPrefix : "",
                unitName,
                string
               );
      }
      return FALSE;
    }
  }
  else
  {
    factor = 1;
  }

  // calculate value
  (*value) = (int)(strtol(number,NULL,0)*factor);

  return TRUE;
}

/***********************************************************************\
* Name   : getInteger64Option
* Purpose: get integer64 option value
* Input  : value         - value variable
*          string        - string
*          name          - option name
*          units         - units array or NULL
*          eutputHandle  - output handle
*          errorPrefix   - error prefix or NULL
*          warningPrefix - warning prefix or NULL
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getInteger64Option(int64                 *value,
                              const char            *string,
                              const char            *name,
                              const CommandLineUnit *units,
                              FILE                  *outputHandle,
                              const char            *errorPrefix,
                              const char            *warningPrefix
                             )
{
  uint                  i,j;
  char                  number[128],unitName[32];
  const CommandLineUnit *unit;
  ulong                 factor;

  assert(value != NULL);
  assert(string != NULL);

  UNUSED_VARIABLE(warningPrefix);

  // split number, unit
  i = strlen(string);
  if (i > 0)
  {
    while ((i > 0) && !isdigit(string[i-1])) { i--; }
//TODO: stringSub
    j = MIN(i,               sizeof(number  )-1); strncpy(number,  &string[0],j); number  [j] = '\0';
    j = MIN(strlen(string)-i,sizeof(unitName)-1); strncpy(unitName,&string[i],j); unitName[j] = '\0';
  }
  else
  {
    number  [0] = '\0';
    unitName[0] = '\0';
  }
  if (number[0] == '\0')
  {
    if (outputHandle != NULL)
    {
      fprintf(outputHandle,
              "%sValue '%s' for option '%s' is not a number!\n",
              (errorPrefix != NULL) ? errorPrefix : "",
              string,
              name
             );
    }
    return FALSE;
  }

  // find unit factor
  if (unitName[0] != '\0')
  {
    if (units != NULL)
    {
      unit = findUnit(units,unitName);
      if (unit == NULL)
      {
        if (outputHandle != NULL)
        {
          fprintf(outputHandle,
                  "%sInvalid unit in integer value '%s'! Valid units:",
                  (errorPrefix != NULL) ? errorPrefix : "",
                  string
                 );
          ITERATE_UNITS(unit,units)
          {
            fprintf(outputHandle," %s",unit->name);
          }
          fprintf(outputHandle,".\n");
        }
        return FALSE;
      }
      factor = unit->factor;
    }
    else
    {
      if (outputHandle != NULL)
      {
        fprintf(outputHandle,
                "%sUnexpected unit '%s' in value '%s'!\n",
                (errorPrefix != NULL) ? errorPrefix : "",
                unitName,
                string
               );
      }
      return FALSE;
    }
  }
  else
  {
    factor = 1LL;
  }

  // calculate value
  (*value) = strtoll(number,NULL,0)*factor;

  return TRUE;
}

/***********************************************************************\
* Name   : processOption
* Purpose: process single command line option
* Input  : commandLineOption - command line option
*          option            - option name
*          value             - option value or NULL
*          outputHandle      - error output handle or NULL
*          errorPrefix       - error prefix or NULL
*          warningPrefix     - warning prefix or NULL
* Output : -
* Return : TRUE if option processed without error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool processOption(const CommandLineOption *commandLineOption,
                         const char              *option,
                         const char              *value,
                         FILE                    *outputHandle,
                         const char              *errorPrefix,
                         const char              *warningPrefix
                        )
{
  char errorMessage[256];

  assert(commandLineOption != NULL);
  assert(option != NULL);

  switch (commandLineOption->type)
  {
    case CMD_OPTION_TYPE_INTEGER:
      {
        assert(commandLineOption->variable.i != NULL);

        // get integer option value
        if (!getIntegerOption(commandLineOption->variable.i,
                              value,
                              option,
                              commandLineOption->integerOption.units,
                              outputHandle,
                              errorPrefix,
                              warningPrefix
                             )
           )
        {
          return FALSE;
        }

        // check range
        if (   ((*commandLineOption->variable.i) < commandLineOption->integerOption.min)
            || ((*commandLineOption->variable.i) > commandLineOption->integerOption.max)
           )
        {
          if (outputHandle != NULL) fprintf(outputHandle,
                                            "%sValue '%s' out of range %d..%d for option '%s'!\n",
                                            (errorPrefix != NULL) ? errorPrefix : "",
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
        assert(commandLineOption->variable.l != NULL);

        // get integer64 option value
        if (!getInteger64Option(commandLineOption->variable.l,
                                value,
                                option,
                                commandLineOption->integer64Option.units,
                                outputHandle,
                                errorPrefix,
                                warningPrefix
                               )
           )
        {
          return FALSE;
        }

        // check range
        if (   ((*commandLineOption->variable.l) < commandLineOption->integer64Option.min)
            || ((*commandLineOption->variable.l) > commandLineOption->integer64Option.max)
           )
        {
          if (outputHandle != NULL) fprintf(outputHandle,
                                                 "%sValue '%s' out of range %"PRIi64"..%"PRIi64" for option '%s'!\n",
                                                 (errorPrefix != NULL) ? errorPrefix : "",
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
        uint                  i,n;
        char                  number[128],unitName[32];
        const CommandLineUnit *unit;
        ulong                 factor;

        assert(commandLineOption->variable.d != NULL);

        // split number, unit
        i = strlen(value);
        if (i > 0)
        {
          while ((i > 0) && !isdigit(value[i-1])) { i--; }
//TODO: stringSub
          n = MIN(i,              sizeof(number  )-1); strncpy(number,  &value[0],n); number  [n] = '\0';
          n = MIN(strlen(value)-i,sizeof(unitName)-1); strncpy(unitName,&value[i],n); unitName[n]   = '\0';
        }
        else
        {
          number  [0] = '\0';
          unitName[0] = '\0';
        }
        if (number[0] == '\0')
        {
          if (outputHandle != NULL)
          {
            fprintf(outputHandle,
                    "%sValue '%s' for option '%s' is not a number!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option
                   );
          }
          return FALSE;
        }

        // find unit factor
        if (unitName[0] != '\0')
        {
          if (commandLineOption->doubleOption.units != NULL)
          {
            unit = findUnit(commandLineOption->doubleOption.units,unitName);
            if (unit == NULL)
            {
              if (outputHandle != NULL)
              {
                fprintf(outputHandle,
                        "%sInvalid unit in float value '%s'! Valid units:",
                        (errorPrefix != NULL)?errorPrefix:"",
                        value
                       );
                ITERATE_UNITS(unit,commandLineOption->integerOption.units)
                {
                  fprintf(outputHandle," %s",unit->name);
                }
                fprintf(outputHandle,".\n");
              }
              return FALSE;
            }
            factor = unit->factor;
          }
          else
          {
            if (outputHandle != NULL)
            {
              fprintf(outputHandle,
                      "%sUnexpected unit '%s' in value '%s'!\n",
                      (errorPrefix != NULL)?errorPrefix:"",
                      unitName,
                      value
                     );
            }
            return FALSE;
          }
        }
        else
        {
          factor = 1L;
        }

        (*commandLineOption->variable.d) = strtod(value,0)*(double)factor;
        if (   ((*commandLineOption->variable.d) < commandLineOption->doubleOption.min)
            || ((*commandLineOption->variable.d) > commandLineOption->doubleOption.max)
           )
        {
          if (outputHandle != NULL) fprintf(outputHandle,
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
               || stringEquals(value,"1")
               || stringEqualsIgnoreCase(value,"true")
               || stringEqualsIgnoreCase(value,"on")
               || stringEqualsIgnoreCase(value,"yes")
              )
      {
        (*commandLineOption->variable.b) = TRUE;
      }
      else if (   stringEquals(value,"0")
               || stringEqualsIgnoreCase(value,"false")
               || stringEqualsIgnoreCase(value,"off")
               || stringEqualsIgnoreCase(value,"no")
              )
      {
        (*commandLineOption->variable.b) = FALSE;
      }
      else
      {
        if (outputHandle != NULL)
        {
          fprintf(outputHandle,
                  "%sInvalid value '%s' for boolean option '%s'!\n",
                  (errorPrefix != NULL)?errorPrefix:"",
                  value,
                  option
                 );
        }
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_FLAG:
      assert(commandLineOption->variable.b != NULL);
      if      (   (value == NULL)
               || stringEquals(value,"1")
               || stringEqualsIgnoreCase(value,"true")
               || stringEqualsIgnoreCase(value,"on")
               || stringEqualsIgnoreCase(value,"yes")
              )
      {
        (*commandLineOption->variable.flags) |= commandLineOption->flagOption.value;
      }
      else if (   stringEquals(value,"0")
               || stringEqualsIgnoreCase(value,"false")
               || stringEqualsIgnoreCase(value,"off")
               || stringEqualsIgnoreCase(value,"no")
              )
      {
        (*commandLineOption->variable.b) &= ~commandLineOption->flagOption.value;
      }
      else
      {
        if (outputHandle != NULL)
        {
          fprintf(outputHandle,
                  "%sInvalid value '%s' for option '%s'!\n",
                  (errorPrefix != NULL)?errorPrefix:"",
                  value,
                  option
                 );
        }
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_INCREMENT:
      assert(commandLineOption->variable.increment != NULL);

      // get/increment option value
      if (value != NULL)
      {
        (*commandLineOption->variable.increment) = (uint)strtol(value,NULL,0);
      }
      else
      {
        (*commandLineOption->variable.increment)++;
      }

      // check range
      if (   ((int)(*commandLineOption->variable.increment) < commandLineOption->incrementOption.min)
          || ((int)(*commandLineOption->variable.increment) > commandLineOption->incrementOption.max)
         )
      {
        if (outputHandle != NULL) fprintf(outputHandle,
                                          "%sValue %d out of range %d..%d for option '%s'!\n",
                                          (errorPrefix != NULL) ? errorPrefix : "",
                                          *commandLineOption->variable.increment,
                                          commandLineOption->incrementOption.min,
                                          commandLineOption->incrementOption.max,
                                          option
                                         );
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_ENUM:
      assert(commandLineOption->variable.enumeration != NULL);
      (*commandLineOption->variable.enumeration) = commandLineOption->enumOption.enumerationValue;
      break;
    case CMD_OPTION_TYPE_SELECT:
      {
        const CommandLineOptionSelect *select;

        assert(commandLineOption->variable.select != NULL);

        select = findSelect(commandLineOption->selectOption.selects,value);
        if (select == NULL)
        {
          if (outputHandle != NULL)
          {
            fprintf(outputHandle,
                    "%sUnknown value '%s' for option '%s'!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option
                   );
          }
          return FALSE;
        }
        (*commandLineOption->variable.select) = select->value;
      }
      break;
    case CMD_OPTION_TYPE_SET:
      {
        uint                       i,j;
        char                       setName[128];
        const CommandLineOptionSet *set;

        assert(commandLineOption->variable.set != NULL);
        i = 0;
        while (value[i] != '\0')
        {
          // skip spaces
          while ((value[i] != '\0') && isspace(value[i])) { i++; }

          // get name
          j = 0;
          while ((value[i] != '\0') && !isspace(value[i]) && (value[i] != ','))
          {
            if (j < sizeof(setName)-1) { setName[j] = value[i]; j++; }
            i++;
          }
          setName[j] = '\0';
          if (value[i] == ',') i++;

          if (setName[0] != '\0')
          {
            // find value
            set = findSet(commandLineOption->setOption.sets,setName);
            if (set == NULL)
            {
              if (outputHandle != NULL)
              {
                fprintf(outputHandle,
                        "%sUnknown value '%s' for option '%s'!\n",
                        (errorPrefix != NULL)?errorPrefix:"",
                        setName,
                        option
                       );
              }
              return FALSE;
            }

            // add to set
            (*commandLineOption->variable.set) |= set->value;
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
        (*commandLineOption->variable.cString) = stringDuplicate(value);
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
      errorMessage[0] = '\0';
      if (!commandLineOption->specialOption.parseSpecial(commandLineOption->specialOption.userData,
                                                         commandLineOption->variable.special,
                                                         option,
                                                         value,
                                                         commandLineOption->defaultValue.special,
                                                         errorMessage,
                                                         sizeof(errorMessage)
                                                        )
         )
      {
        if (outputHandle != NULL)
        {
          errorMessage[sizeof(errorMessage)-1] = '\0';
          if (strlen(errorMessage) > 0)
          {
            fprintf(outputHandle,
                    "%sInvalid value '%s' for option '%s' (error: %s)!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option,
                    errorMessage
                   );
          }
          else
          {
            fprintf(outputHandle,
                    "%sInvalid value '%s' for option '%s'!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option
                   );
          }
        }
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_DEPRECATED:
      errorMessage[0] = '\0';
      if (outputHandle != NULL)
      {
        if (commandLineOption->deprecatedOption.newOptionName != NULL)
        {
          fprintf(outputHandle,
                  "%sOption '%s' is deprecated. Please use '%s' instead.\n",
                  (warningPrefix != NULL)?warningPrefix:"",
                  option,
                  commandLineOption->deprecatedOption.newOptionName
                 );
        }
        else
        {
          fprintf(outputHandle,
                  "%sOption '%s' is deprecated.\n",
                  (warningPrefix != NULL)?warningPrefix:"",
                  option
                 );
        }
      }
      if (!commandLineOption->deprecatedOption.parseDeprecated(commandLineOption->deprecatedOption.userData,
                                                               commandLineOption->variable.special,
                                                               option,
                                                               value,
                                                               commandLineOption->defaultValue.special,
                                                               errorMessage,
                                                               sizeof(errorMessage)
                                                              )
         )
      {
        if (outputHandle != NULL)
        {
          errorMessage[sizeof(errorMessage)-1] = '\0';
          if (strlen(errorMessage) > 0)
          {
            fprintf(outputHandle,
                    "%sInvalid value '%s' for deprecated option '%s' (error: %s)!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option,
                    errorMessage
                   );
          }
          else
          {
            fprintf(outputHandle,
                    "%sInvalid value '%s' for deprecated option '%s'!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    value,
                    option
                   );
          }
        }
        return FALSE;
      }
      break;
    case CMD_OPTION_TYPE_END:
      break;
  }

  if (!Array_contains(&cmdSetOptions,commandLineOption->variable.pointer))
  {
    Array_append(&cmdSetOptions,commandLineOption->variable.pointer);
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

  uint   i;
  size_t bytesWritten;

  assert(outputHandle != NULL);

  i = 0;
  while ((i+8) < n)
  {
    bytesWritten = fwrite(SPACES8,1,8,outputHandle);
    i += 8;
  }
  while (i < n)
  {
    (void)fputc(' ',outputHandle);
    i++;
  }

  UNUSED_VARIABLE(bytesWritten);
}

/***********************************************************************\
* Name   : expandMacros
* Purpose: expand macros
* Input  : line              - line variable
*          lineSize          - line variable size
*          template          - template
*          templateLength    - length of template
*          commandLineOption - command line option
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void expandMacros(char *line, uint lineSize, const char *template, uint templateLength, const CommandLineOption *commandLineOption)
{
  #define APPEND(s,length,i,ch) \
    do \
    { \
      if (i < ((length)-1)) \
      { \
        s[i] = ch; (i)++; \
        s[i] = NUL; \
      } \
    } \
    while (0)

  assert(line != NULL);
  assert(template != NULL);
  assert(commandLineOption != NULL);

  uint i = 0;
  uint j = 0;
  while (i < templateLength)
  {
    if (template[i] == '%')
    {
      i++;
      if (template[i] == '%')
      {
        // %% -> %
        APPEND(line,lineSize,j,'%');
        i++;
      }
      else
      {
        // %...%

        // get macro name
        char macro[64];
        uint k = 0;
        while ((i < templateLength) && (template[i] != '%'))
        {
          APPEND(macro,sizeof(macro),k,template[i]);
          i++;
        }
        i++;

        // expand macro
        if (stringEquals(macro,"default"))
        {
          switch (commandLineOption->type)
          {
            case CMD_OPTION_TYPE_INTEGER:
              if ((commandLineOption->integerOption.min < commandLineOption->defaultValue.i) && (commandLineOption->defaultValue.i < commandLineOption->integerOption.max))
              {
                const CommandLineUnit *unit = findIntegerUnitByValue(commandLineOption->integerOption.units,commandLineOption->defaultValue.i);
                if (unit != NULL)
                {
                  stringAppendFormat(line,lineSize,"%d%s",commandLineOption->defaultValue.i/(int)unit->factor,unit->name);
                }
                else
                {
                  stringAppendFormat(line,lineSize,"%d",commandLineOption->defaultValue.i);
                }
              }
              else
              {
                stringAppendFormat(line,lineSize,"%d",commandLineOption->defaultValue.i);
              }
              break;
            case CMD_OPTION_TYPE_INTEGER64:
              if ((commandLineOption->integer64Option.min < commandLineOption->defaultValue.l) && (commandLineOption->defaultValue.l < commandLineOption->integer64Option.max))
              {
                const CommandLineUnit *unit = findInteger64UnitByValue(commandLineOption->integer64Option.units,commandLineOption->defaultValue.l);
                if (unit != NULL)
                {
                  stringAppendFormat(line,lineSize,"%"PRIi64"%s",commandLineOption->defaultValue.l/unit->factor,unit->name);
                }
                else
                {
                  stringAppendFormat(line,lineSize,"%"PRIi64,commandLineOption->defaultValue.l);
                }
              }
              else
              {
                stringAppendFormat(line,lineSize,"%"PRIi64,commandLineOption->defaultValue.l);
              }
              break;
            case CMD_OPTION_TYPE_DOUBLE:
              stringAppendFormat(line,lineSize,"%lf",commandLineOption->defaultValue.d);
              break;
            case CMD_OPTION_TYPE_BOOLEAN:
              break;
            case CMD_OPTION_TYPE_FLAG:
              break;
            case CMD_OPTION_TYPE_INCREMENT:
              break;
            case CMD_OPTION_TYPE_ENUM:
              break;
            case CMD_OPTION_TYPE_SELECT:
              break;
            case CMD_OPTION_TYPE_SET:
              break;
            case CMD_OPTION_TYPE_CSTRING:
              stringAppend(line,lineSize,commandLineOption->defaultValue.cString);
              break;
            case CMD_OPTION_TYPE_STRING:
              stringAppend(line,lineSize,String_cString(commandLineOption->defaultValue.string));
              break;
            case CMD_OPTION_TYPE_SPECIAL:
              break;
            case CMD_OPTION_TYPE_DEPRECATED:
              break;
            case CMD_OPTION_TYPE_END:
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break;
            #endif /* NDEBUG */
          }
          j = stringLength(line);
        }
      }
    }
    else
    {
      APPEND(line,lineSize,j,template[i]);
      i++;
    }
  }

  #undef APPEND
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
  bool CmdOption_init(CommandLineOption commandLineOptions[]
                     )
#else /* not NDEBUG */
  bool __CmdOption_init(const char        *__fileName__,
                        ulong             __lineNb__,
                        CommandLineOption commandLineOptions[]
                       )
#endif /* NDEBUG */
{
  uint i;
  #ifndef NDEBUG
    uint j;
  #endif /* NDEBUG */

  assert(commandLineOptions != NULL);

  #ifndef NDEBUG
    // check for duplicate names
    for (i = 0; commandLineOptions[i].type != CMD_OPTION_TYPE_END; i++)
    {
      for (j = 0; commandLineOptions[j].type != CMD_OPTION_TYPE_END; j++)
      {
        if (i != j)
        {
          if (stringEquals(commandLineOptions[i].name,commandLineOptions[j].name))
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

  /* get default values from initial settings of variables
     Note: strings are always new allocated and reallocated in CmdOption_parse() resp. freed in CmdOption_init()
  */
  for (i = 0; commandLineOptions[i].type != CMD_OPTION_TYPE_END; i++)
  {
    assert(commandLineOptions[i].variable.pointer != NULL);
    switch (commandLineOptions[i].type)
    {
      case CMD_OPTION_TYPE_INTEGER:
        assertx(commandLineOptions[i].variable.i != NULL,"%s",commandLineOptions[i].name);
        assertx((*commandLineOptions[i].variable.i) >= commandLineOptions[i].integerOption.min,"%s %d >= %d",commandLineOptions[i].name,*commandLineOptions[i].variable.i,commandLineOptions[i].integerOption.min);
        assertx((*commandLineOptions[i].variable.i) <= commandLineOptions[i].integerOption.max,"%s %d <= %d",commandLineOptions[i].name,*commandLineOptions[i].variable.i,commandLineOptions[i].integerOption.max);
        commandLineOptions[i].defaultValue.i = (*commandLineOptions[i].variable.i);
        break;
      case CMD_OPTION_TYPE_INTEGER64:
        assertx(commandLineOptions[i].variable.l != NULL,"%s",commandLineOptions[i].name);
        assertx((*commandLineOptions[i].variable.l) >= commandLineOptions[i].integer64Option.min,"%s %"PRIi64" >= %"PRIi64,commandLineOptions[i].name,*commandLineOptions[i].variable.l,commandLineOptions[i].integer64Option.min);
        assertx((*commandLineOptions[i].variable.l) <= commandLineOptions[i].integer64Option.max,"%s %"PRIi64" <= %"PRIi64,commandLineOptions[i].name,*commandLineOptions[i].variable.l,commandLineOptions[i].integer64Option.max);
        commandLineOptions[i].defaultValue.l = (*commandLineOptions[i].variable.l);
        break;
      case CMD_OPTION_TYPE_DOUBLE:
        assertx(commandLineOptions[i].variable.d != NULL,"%s",commandLineOptions[i].name);
        assertx((*commandLineOptions[i].variable.d)+EPSILON_DOUBLE >= commandLineOptions[i].doubleOption.min,"%s %lf >= %lf",commandLineOptions[i].name,*commandLineOptions[i].variable.d,commandLineOptions[i].doubleOption.min);
        assertx((*commandLineOptions[i].variable.d)-EPSILON_DOUBLE <= commandLineOptions[i].doubleOption.max,"%s %lf <= %lf",commandLineOptions[i].name,*commandLineOptions[i].variable.d,commandLineOptions[i].doubleOption.max);
        commandLineOptions[i].defaultValue.d = (*commandLineOptions[i].variable.d);
        break;
      case CMD_OPTION_TYPE_BOOLEAN:
        assertx(commandLineOptions[i].variable.b != NULL,"%s",commandLineOptions[i].name);
        assertx(   ((*commandLineOptions[i].variable.b) == TRUE )
                || ((*commandLineOptions[i].variable.b) == FALSE),
                "%s",commandLineOptions[i].name
               );
        commandLineOptions[i].defaultValue.b = (*commandLineOptions[i].variable.b);
        break;
      case CMD_OPTION_TYPE_FLAG:
        assertx(commandLineOptions[i].variable.flags != NULL,"%s",commandLineOptions[i].name);
        commandLineOptions[i].defaultValue.flags = (*commandLineOptions[i].variable.flags);
        break;
      case CMD_OPTION_TYPE_INCREMENT:
        assertx(commandLineOptions[i].variable.increment != NULL,"%s",commandLineOptions[i].name);
        commandLineOptions[i].defaultValue.increment = (*commandLineOptions[i].variable.increment);
        break;
      case CMD_OPTION_TYPE_ENUM:
        assertx(commandLineOptions[i].variable.enumeration != NULL,"%s",commandLineOptions[i].name);
        commandLineOptions[i].defaultValue.enumeration = (*commandLineOptions[i].variable.enumeration);
        break;
      case CMD_OPTION_TYPE_SELECT:
        assertx(commandLineOptions[i].variable.select != NULL,"%s",commandLineOptions[i].name);
        commandLineOptions[i].defaultValue.select = (*commandLineOptions[i].variable.select);
        break;
      case CMD_OPTION_TYPE_SET:
        assertx(commandLineOptions[i].variable.set != NULL,"%s",commandLineOptions[i].name);
        commandLineOptions[i].defaultValue.set = (*commandLineOptions[i].variable.set);
        break;
      case CMD_OPTION_TYPE_CSTRING:
        assertx(commandLineOptions[i].variable.cString != NULL,"%s",commandLineOptions[i].name);
        if ((*commandLineOptions[i].variable.cString) != NULL)
        {
          commandLineOptions[i].defaultValue.cString = (*commandLineOptions[i].variable.cString);
          (*commandLineOptions[i].variable.cString) = stringDuplicate(commandLineOptions[i].defaultValue.cString);
        }
        else
        {
          commandLineOptions[i].defaultValue.cString = NULL;
        }
        break;
      case CMD_OPTION_TYPE_STRING:
        assertx(commandLineOptions[i].variable.string != NULL,"%s",commandLineOptions[i].name);
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
      case CMD_OPTION_TYPE_DEPRECATED:
        commandLineOptions[i].defaultValue.deprecated = commandLineOptions[i].variable.deprecated;
        break;
      case CMD_OPTION_TYPE_END:
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
  }

  Array_init(&cmdSetOptions,sizeof(void*),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(commandLineOptions,CommandLineOptions);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,commandLineOptions,CommandLineOptions);
  #endif /* NDEBUG */

  return TRUE;
}

#ifdef NDEBUG
  void CmdOption_done(CommandLineOption commandLineOptions[]
                     )
#else /* not NDEBUG */
  void __CmdOption_done(const char        *__fileName__,
                        ulong             __lineNb__,
                        CommandLineOption commandLineOptions[]
                       )
#endif /* NDEBUG */
{
  uint i;

  assert(commandLineOptions != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(commandLineOptions,CommandLineOptions);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,commandLineOptions,CommandLineOptions);
  #endif /* NDEBUG */

  Array_done(&cmdSetOptions);

  // free values and restore from default values
  for (i = 0; commandLineOptions[i].type != CMD_OPTION_TYPE_END; i++)
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
      case CMD_OPTION_TYPE_FLAG:
        break;
      case CMD_OPTION_TYPE_INCREMENT:
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
      case CMD_OPTION_TYPE_DEPRECATED:
        break;
      case CMD_OPTION_TYPE_END:
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
                     uint                    minPriority,
                     uint                    maxPriority,
                     FILE                    *outputHandle,
                     const char              *errorPrefix,
                     const char              *warningPrefix
                    )
{
  bool       collectArgumentsFlag;
  uint       i;
  uint       priority;
  bool       endOfOptionsFlag;
  const char *s;
  char       name[128];
  char       option[128];
  uint       j;
  const char *optionChars;
  const char *value;
  int        argumentsCount;

  assert(argv != NULL);
  assert(argc != NULL);
  assert((*argc) >= 1);
  assert(commandLineOptions != NULL);

  // get min./max. option priority, set arguments collect flag
  collectArgumentsFlag = FALSE;
  if (minPriority == CMD_PRIORITY_ANY)
  {
    minPriority = MAX_UINT;
    for (i = 0; commandLineOptions[i].type != CMD_OPTION_TYPE_END; i++)
    {
      minPriority = MIN(minPriority,commandLineOptions[i].priority);
    }
  }
  if (maxPriority == CMD_PRIORITY_ANY)
  {
    maxPriority = 0;
    for (i = 0; commandLineOptions[i].type != CMD_OPTION_TYPE_END; i++)
    {
      maxPriority = MAX(maxPriority,commandLineOptions[i].priority);
    }
    collectArgumentsFlag = TRUE;
  }

  // reset increment options
  for (i = 0; commandLineOptions[i].type != CMD_OPTION_TYPE_END; i++)
  {
    if (   (commandLineOptions[i].type == CMD_OPTION_TYPE_INCREMENT)
        && IS_IN_RANGE(minPriority,commandLineOptions[i].priority,maxPriority)
       )
    {
      assert(commandLineOptions[i].variable.increment != NULL);
      (*commandLineOptions[i].variable.increment) = commandLineOptions[i].defaultValue.increment;
    }
  }

  // parse options
  argumentsCount = 1;
  for (priority = minPriority; priority <= maxPriority; priority++)
  {
    endOfOptionsFlag = FALSE;
    i = 1;
    while (i < (uint)(*argc))
    {
      if      (!endOfOptionsFlag && stringEquals(argv[i],"--"))
      {
        endOfOptionsFlag = TRUE;
      }
      else if (!endOfOptionsFlag && stringStartsWith(argv[i],"--"))
      {
        // get name
        s = strchr(argv[i]+2,'=');
        if (s != NULL)
        {
//TODO: stringSub
          strncpy(name,argv[i]+2,MIN((uint)(s-(argv[i]+2)),sizeof(name)-1));
          name[MIN((uint)(s-(argv[i]+2)),sizeof(name)-1)] = '\0';
        }
        else
        {
//TODO: stringSub
          strncpy(name,argv[i]+2,sizeof(name)-1);
          name[sizeof(name)-1] = '\0';
        }

        // find option
        j = 0;
        while ((commandLineOptions[j].type != CMD_OPTION_TYPE_END) && !stringEquals(commandLineOptions[j].name,name))
        {
          j++;
        }
        if (commandLineOptions[j].type != CMD_OPTION_TYPE_END)
        {
          // get option value
          value = NULL;
          switch (commandLineOptions[j].type)
          {
            case CMD_OPTION_TYPE_INTEGER:
            case CMD_OPTION_TYPE_INTEGER64:
            case CMD_OPTION_TYPE_DOUBLE:
            case CMD_OPTION_TYPE_SELECT:
            case CMD_OPTION_TYPE_SET:
            case CMD_OPTION_TYPE_CSTRING:
            case CMD_OPTION_TYPE_STRING:
              if (s != NULL)
              {
                // skip '='
                s++;
                value = s;
              }
              else
              {
                if      ((i+1) < (uint)(*argc))
                {
                  // get value
                  i++;
                  value = argv[i];
                }
                else
                {
                  // missing value for option
                  if (outputHandle != NULL)
                  {
                    fprintf(outputHandle,
                            "%sNo value given for option '--%s'!\n",
                            (errorPrefix != NULL)?errorPrefix:"",
                            name
                           );
                  }
                  return FALSE;
                }
              }
              break;
            case CMD_OPTION_TYPE_BOOLEAN:
              if (s != NULL)
              {
                // skip '='
                s++;
                value = s;
              }
              break;
            case CMD_OPTION_TYPE_FLAG:
              if (s != NULL)
              {
                // skip '='
                s++;
                value = s;
              }
              break;
            case CMD_OPTION_TYPE_INCREMENT:
              if (s != NULL)
              {
                // skip '='
                s++;
                value = s;
              }
              break;
            case CMD_OPTION_TYPE_ENUM:
              value = NULL;
              break;
            case CMD_OPTION_TYPE_SPECIAL:
              assert(commandLineOptions[j].specialOption.argumentCount <= 1);

              if      (s != NULL)
              {
                // skip '='
                s++;
                value = s;
              }
              else if (commandLineOptions[j].specialOption.argumentCount > 0)
              {
                if      ((i+1) < (uint)(*argc))
                {
                  // get value
                  i++;
                  value = argv[i];
                }
                else
                {
                  // missing value for option
                  if (outputHandle != NULL)
                  {
                    fprintf(outputHandle,
                            "%sNo value given for option '--%s'!\n",
                            (errorPrefix != NULL)?errorPrefix:"",
                            name
                           );
                  }
                  return FALSE;
                }
              }
              else
              {
                // no value
                value = NULL;
              }
              break;
            case CMD_OPTION_TYPE_DEPRECATED:
              assert(commandLineOptions[j].deprecatedOption.argumentCount <= 1);

              if      (s != NULL)
              {
                // skip '='
                s++;
                value = s;
              }
              else if (commandLineOptions[j].deprecatedOption.argumentCount > 0)
              {
                if      ((i+1) < (uint)(*argc))
                {
                  // get value
                  i++;
                  value = argv[i];
                }
                else
                {
                  // missing value for option
                  if (outputHandle != NULL)
                  {
                    fprintf(outputHandle,
                            "%sNo value given for option '--%s'!\n",
                            (errorPrefix != NULL)?errorPrefix:"",
                            name
                           );
                  }
                  return FALSE;
                }
              }
              else
              {
                // no value
                value = NULL;
              }
              break;
            case CMD_OPTION_TYPE_END:
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break;
            #endif /* NDEBUG */
          }

          if (commandLineOptions[j].priority == priority)
          {
            // process option
            stringFormat(option,sizeof(option),"--%s",name);
            if (!processOption(&commandLineOptions[j],option,value,outputHandle,errorPrefix,warningPrefix))
            {
              return FALSE;
            }
          }
        }
        else
        {
          if (outputHandle != NULL)
          {
            fprintf(outputHandle,
                    "%sUnknown option '--%s'!\n",
                    (errorPrefix != NULL)?errorPrefix:"",
                    name
                   );
          }
          return FALSE;
        }
      }
      else if (!endOfOptionsFlag && stringStartsWith(argv[i],"-"))
      {
        // get option chars
        optionChars = argv[i]+1;
        while ((optionChars != NULL) && ((*optionChars) != NUL))
        {
          // get short name
          name[0] = optionChars[0];
          name[1] = '\0';

          // find option
          j = 0;
          while ((commandLineOptions[j].type != CMD_OPTION_TYPE_END) && (commandLineOptions[j].shortName != name[0]))
          {
            j++;
          }
          if (commandLineOptions[j].type != CMD_OPTION_TYPE_END)
          {
            // find optional value for option
            value = NULL;
            switch (commandLineOptions[j].type)
            {
              case CMD_OPTION_TYPE_INTEGER:
              case CMD_OPTION_TYPE_INTEGER64:
              case CMD_OPTION_TYPE_DOUBLE:
              case CMD_OPTION_TYPE_SELECT:
              case CMD_OPTION_TYPE_SET:
              case CMD_OPTION_TYPE_CSTRING:
              case CMD_OPTION_TYPE_STRING:
                // next argument is option value
                if ((i+1) >= (uint)(*argc))
                {
                  if (outputHandle != NULL)
                  {
                    fprintf(outputHandle,
                            "%sNo value given for option '-%s'!\n",
                            (errorPrefix != NULL)?errorPrefix:"",
                            name
                           );
                  }
                  return FALSE;
                }
                i++;
                value = argv[i];
                break;
              case CMD_OPTION_TYPE_BOOLEAN:
              case CMD_OPTION_TYPE_FLAG:
                value = NULL;
                break;
              case CMD_OPTION_TYPE_INCREMENT:
                // check if '=' follow
                if (optionChars[1] == '=')
                {
                  value = &optionChars[2];
                  optionChars = NULL;
                }
                else
                {
                  value = NULL;
                }
                break;
              case CMD_OPTION_TYPE_ENUM:
                value = NULL;
                break;
              case CMD_OPTION_TYPE_SPECIAL:
                assert(commandLineOptions[j].specialOption.argumentCount <= 1);

                if (commandLineOptions[j].specialOption.argumentCount > 0)
                {
                  // next argument is option value
                  if ((i+1) >= (uint)(*argc))
                  {
                    if (outputHandle != NULL)
                    {
                      fprintf(outputHandle,
                              "%sNo value given for option '-%s'!\n",
                              (errorPrefix != NULL)?errorPrefix:"",
                              name
                             );
                    }
                    return FALSE;
                  }
                  i++;
                  value = argv[i];
                }
                else
                {
                  // no value
                  value = NULL;
                }
                break;
              case CMD_OPTION_TYPE_DEPRECATED:
                assert(commandLineOptions[j].deprecatedOption.argumentCount <= 1);

                if (commandLineOptions[j].deprecatedOption.argumentCount > 0)
                {
                  // next argument is option value
                  if ((i+1) >= (uint)(*argc))
                  {
                    if (outputHandle != NULL)
                    {
                      fprintf(outputHandle,
                              "%sNo value given for option '-%s'!\n",
                              (errorPrefix != NULL)?errorPrefix:"",
                              name
                             );
                    }
                    return FALSE;
                  }
                  i++;
                  value = argv[i];
                }
                else
                {
                  // no value
                  value = NULL;
                }
                break;
              case CMD_OPTION_TYPE_END:
                break;
            }

            if (commandLineOptions[j].priority == priority)
            {
              // process option
              stringFormat(option,sizeof(option),"-%s",name);
              if (!processOption(&commandLineOptions[j],option,value,outputHandle,errorPrefix,warningPrefix))
              {
                return FALSE;
              }
            }

            // next option char
            if (optionChars != NULL) optionChars++;
          }
          else
          {
            if (outputHandle != NULL)
            {
              fprintf(outputHandle,
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
        if (collectArgumentsFlag && (priority >= maxPriority))
        {
          // add argument
          argv[argumentsCount] = argv[i];
          argumentsCount++;
        }
      }

      i++;
    }
  }
  if (collectArgumentsFlag)
  {
    (*argc) = argumentsCount;
  }

  return TRUE;
}

bool CmdOption_parseDeprecatedStringOption(void       *userData,
                                           void       *variable,
                                           const char *name,
                                           const char *value,
                                           const void *defaultValue,
                                           char       errorMessage[],
                                           uint       errorMessageSize
                                          )
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  String_setCString(*((String*)variable),value);

  return TRUE;
}

bool CmdOption_parseDeprecatedCStringOption(void       *userData,
                                            void       *variable,
                                            const char *name,
                                            const char *value,
                                            const void *defaultValue,
                                            char       errorMessage[],
                                            uint       errorMessageSize
                                           )
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

//TODO: correct?
  (*((const char**)variable)) = value;

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
  while ((i < commandLineOptionCount) && !stringEquals(commandLineOptions[i].name,name))
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

  return processOption(commandLineOption,commandLineOption->name,value,NULL,NULL,NULL);
}

bool CmdOption_getIntegerOption(int                   *value,
                                const char            *string,
                                const char            *option,
                                const CommandLineUnit *units
                               )
{
  return getIntegerOption(value,string,option,units,NULL,NULL,NULL);
}

bool CmdOption_getInteger64Option(int64                 *value,
                                  const char            *string,
                                  const char            *option,
                                  const CommandLineUnit *units
                                 )
{
  return getInteger64Option(value,string,option,units,NULL,NULL,NULL);
}

const char *CmdOption_selectToString(const CommandLineOptionSelect selects[],
                                     uint                          value,
                                     const char                    *defaultString
                                    )
{
  const CommandLineOptionSelect *select;

  assert(selects != NULL);

  ITERATE_SELECT(select,selects)
  {
    if (select->value == value) return select->name;
  }

  return defaultString;
}

// try to avoid warning because of == operation on double/float (valid here, because it is the initial value)
#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif /* __GNUC__ */

void CmdOption_printHelp(FILE                    *outputHandle,
                         const CommandLineOption commandLineOptions[],
                         int                     helpLevel
                        )
{
  #define PREFIX "Options: "

  uint                          i;
  uint                          maxNameLength;
  uint                          n;
  char                          name[128];
  uint                          j;
  const CommandLineUnit         *unit;
  char                          s[6];
  uint                          maxValueLength;
  const char                    *token;
  const char                    *separator;
  char                          description[256];
  const CommandLineOptionSelect *select;
  const CommandLineOptionSet    *set;

  assert(outputHandle != NULL);
  assert(commandLineOptions != NULL);

  // get max. width of name column
  maxNameLength = 0;
  for (i = 0; commandLineOptions[i].type != CMD_OPTION_TYPE_END; i++)
  {
    assert(commandLineOptions[i].name != NULL);

    if ((helpLevel == CMD_HELP_LEVEL_ALL) || (helpLevel >= (int)commandLineOptions[i].helpLevel))
    {
      n = 0;

      // short name length
      if (commandLineOptions[i].shortName != '\0')
      {
        n += 3; // "-x|"
      }

      // name length
      n += 2 + strlen(commandLineOptions[i].name); // --name

      // value length
      switch (commandLineOptions[i].type)
      {
        case CMD_OPTION_TYPE_INTEGER:
          n += 4; // =<n>
          if (commandLineOptions[i].integerOption.units != NULL)
          {
            n += 1; // [
            j = 0;
            ITERATE_UNITS(unit,commandLineOptions[i].integerOption.units)
            {
              if (j > 0) n += 1; // |
              n += strlen(unit->name); // unit
              j++;
            }
            n += 1; // ]
          }
          break;
        case CMD_OPTION_TYPE_INTEGER64:
          n += 4; // =<n>
          if (commandLineOptions[i].integer64Option.units != NULL)
          {
            n += 1; // [
            j = 0;
            ITERATE_UNITS(unit,commandLineOptions[i].integer64Option.units)
            {
              if (j > 0) n += 1; // |
              n += strlen(unit->name); // unit
              j++;
            }
            n += 1; // ]
          }
          break;
        case CMD_OPTION_TYPE_DOUBLE:
          n += 4; // =<n>
          break;
        case CMD_OPTION_TYPE_BOOLEAN:
          if (commandLineOptions[i].booleanOption.yesnoFlag)
          {
            n += 9; // [=yes|no]
          }
          break;
        case CMD_OPTION_TYPE_FLAG:
          if (commandLineOptions[i].booleanOption.yesnoFlag)
          {
            n += 9; // [=yes|no]
          }
          break;
        case CMD_OPTION_TYPE_INCREMENT:
          break;
        case CMD_OPTION_TYPE_ENUM:
          break;
        case CMD_OPTION_TYPE_SELECT:
          n += 2; // =<
          if (commandLineOptions[i].selectOption.descriptionArgument != NULL)
          {
            n += strlen(commandLineOptions[i].selectOption.descriptionArgument);
          }
          else
          {
            n += 4; // name
          }
          n += 1; // >
          break;
        case CMD_OPTION_TYPE_SET:
          n += 2; // =<
          if (commandLineOptions[i].stringOption.descriptionArgument != NULL)
          {
            n += strlen(commandLineOptions[i].stringOption.descriptionArgument);
            n += 3; // ,[<
            n += strlen(commandLineOptions[i].stringOption.descriptionArgument);
            n += 5; // >...]
          }
          else
          {
            n += 18; // name>[,<name>...]
          }
          n += 1; // >
          break;
        case CMD_OPTION_TYPE_CSTRING:
        case CMD_OPTION_TYPE_STRING:
          n += 2; // =<
          if (commandLineOptions[i].stringOption.descriptionArgument != NULL)
          {
            n += strlen(commandLineOptions[i].stringOption.descriptionArgument);
          }
          else
          {
            n += 6; // string
          }
          n += 1; // >
          break;
        case CMD_OPTION_TYPE_SPECIAL:
          if (commandLineOptions[i].specialOption.argumentCount > 0)
          {
            n += 2; // =<
            if (commandLineOptions[i].specialOption.descriptionArgument != NULL)
            {
              n += strlen(commandLineOptions[i].specialOption.descriptionArgument);
            }
            else
            {
              n += 3; // ...
            }
            n += 1; // >
          }
          break;
        case CMD_OPTION_TYPE_DEPRECATED:
          if (commandLineOptions[i].deprecatedOption.argumentCount > 0)
          {
            n += 2+3+1; // =<...>
          }
          break;
        case CMD_OPTION_TYPE_END:
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

  // output help
  for (i = 0; commandLineOptions[i].type != CMD_OPTION_TYPE_END; i++)
  {
    if (   (commandLineOptions[i].type != CMD_OPTION_TYPE_DEPRECATED)
        && ((helpLevel == CMD_HELP_LEVEL_ALL) || (helpLevel >= (int)commandLineOptions[i].helpLevel))
       )
    {
      // output prefix
      if (i == 0)
      {
        (void)fputs(PREFIX,outputHandle);
      }
      else
      {
        printSpaces(outputHandle,strlen(PREFIX));
      }

      // output name
      stringClear(name);
      if (commandLineOptions[i].shortName != '\0')
      {
        stringFormat(s,sizeof(s)-1,"-%c|",commandLineOptions[i].shortName);
        stringAppend(name,sizeof(name),s);
      }
      assert(commandLineOptions[i].name != NULL);
      stringAppend(name,sizeof(name),"--");
      stringAppend(name,sizeof(name),commandLineOptions[i].name);
      switch (commandLineOptions[i].type)
      {
        case CMD_OPTION_TYPE_INTEGER:
          stringAppend(name,sizeof(name),"=<n>");
          if (commandLineOptions[i].integerOption.units != NULL)
          {
            stringAppend(name,sizeof(name),"[");
            j = 0;
            ITERATE_UNITS(unit,commandLineOptions[i].integerOption.units)
            {
              if (j > 0) stringAppend(name,sizeof(name),"|");
              stringAppend(name,sizeof(name),unit->name);
              j++;
            }
            stringAppend(name,sizeof(name),"]");
          }
          break;
        case CMD_OPTION_TYPE_INTEGER64:
          stringAppend(name,sizeof(name),"=<n>");
          if (commandLineOptions[i].integer64Option.units != NULL)
          {
            stringAppend(name,sizeof(name),"[");
            j = 0;
            ITERATE_UNITS(unit,commandLineOptions[i].integer64Option.units)
            {
              if (j > 0) stringAppend(name,sizeof(name),"|");
              stringAppend(name,sizeof(name),unit->name);
              j++;
            }
            stringAppend(name,sizeof(name),"]");
          }
          break;
        case CMD_OPTION_TYPE_DOUBLE:
          stringAppend(name,sizeof(name),"=<n>");
          break;
        case CMD_OPTION_TYPE_BOOLEAN:
          if (commandLineOptions[i].booleanOption.yesnoFlag)
          {
            stringAppend(name,sizeof(name),"[=yes|no]");
          }
          break;
        case CMD_OPTION_TYPE_FLAG:
          if (commandLineOptions[i].booleanOption.yesnoFlag)
          {
            stringAppend(name,sizeof(name),"[=yes|no]");
          }
          break;
        case CMD_OPTION_TYPE_INCREMENT:
          break;
        case CMD_OPTION_TYPE_ENUM:
          break;
        case CMD_OPTION_TYPE_SELECT:
          stringAppend(name,sizeof(name),"=<");
          if (commandLineOptions[i].selectOption.descriptionArgument != NULL)
          {
            stringAppend(name,sizeof(name),commandLineOptions[i].selectOption.descriptionArgument);
          }
          else
          {
            stringAppend(name,sizeof(name),"name");
          }
          stringAppend(name,sizeof(name),">");
          break;
        case CMD_OPTION_TYPE_SET:
          stringAppend(name,sizeof(name),"=<");
          if (commandLineOptions[i].setOption.descriptionArgument != NULL)
          {
            stringAppend(name,sizeof(name),commandLineOptions[i].setOption.descriptionArgument);
            stringAppend(name,sizeof(name),"[,<");
            stringAppend(name,sizeof(name),commandLineOptions[i].setOption.descriptionArgument);
            stringAppend(name,sizeof(name),">...]");
          }
          else
          {
            stringAppend(name,sizeof(name),"name>[,<name>...]");
          }
          stringAppend(name,sizeof(name),">");
          break;
        case CMD_OPTION_TYPE_CSTRING:
        case CMD_OPTION_TYPE_STRING:
          stringAppend(name,sizeof(name),"=<");
          if (commandLineOptions[i].stringOption.descriptionArgument != NULL)
          {
            stringAppend(name,sizeof(name),commandLineOptions[i].stringOption.descriptionArgument);
          }
          else
          {
            stringAppend(name,sizeof(name),"string");
          }
          stringAppend(name,sizeof(name),">");
          break;
        case CMD_OPTION_TYPE_SPECIAL:
          if (commandLineOptions[i].specialOption.argumentCount > 0)
          {
            stringAppend(name,sizeof(name),"=<");
            if (commandLineOptions[i].specialOption.descriptionArgument != NULL)
            {
              stringAppend(name,sizeof(name),commandLineOptions[i].specialOption.descriptionArgument);
            }
            else
            {
              stringAppend(name,sizeof(name),"...");
            }
            stringAppend(name,sizeof(name),">");
          }
          break;
        case CMD_OPTION_TYPE_DEPRECATED:
          if (commandLineOptions[i].deprecatedOption.argumentCount > 0)
          {
            stringAppend(name,sizeof(name),"=<...>");
          }
          break;
        case CMD_OPTION_TYPE_END:
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      (void)fputs(name,outputHandle);

      // output descriptions
      assert(maxNameLength >= strlen(name));
      printSpaces(outputHandle,maxNameLength-strlen(name));
      if (commandLineOptions[i].description != NULL)
      {
        // get description
        token = commandLineOptions[i].description;
        separator = strchr(token,'\n');

        // output description
        if (separator != NULL)
        {
          do
          {
            expandMacros(description,sizeof(description),token,separator-token,&commandLineOptions[i]);

            UNUSED_RESULT(fputc(' ',outputHandle));
            UNUSED_RESULT(fwrite(description,1,stringLength(description),outputHandle));
            UNUSED_RESULT(fputc('\n',outputHandle));
            printSpaces(outputHandle,strlen(PREFIX)+maxNameLength);

            token = separator+1;
            separator = strchr(token,'\n');
          }
          while (separator != NULL);
        }
        expandMacros(description,sizeof(description),token,stringLength(token),&commandLineOptions[i]);
        UNUSED_RESULT(fputc(' ',outputHandle));
        UNUSED_RESULT(fputs(description,outputHandle));
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
            (void)fputc(')',outputHandle);
          }
          break;
        case CMD_OPTION_TYPE_INTEGER64:
          if (commandLineOptions[i].integer64Option.rangeFlag)
          {
            if      ((commandLineOptions[i].integer64Option.min > INT64_MIN) && (commandLineOptions[i].integer64Option.max < INT64_MAX))
            {
              fprintf(outputHandle," (%"PRIi64"..%"PRIi64,commandLineOptions[i].integer64Option.min,commandLineOptions[i].integer64Option.max);
            }
            else if (commandLineOptions[i].integer64Option.min > INT64_MIN)
            {
              fprintf(outputHandle," (>= %"PRIi64,commandLineOptions[i].integer64Option.min);
            }
            else if (commandLineOptions[i].integer64Option.max < INT64_MAX)
            {
              fprintf(outputHandle," (<= %"PRIi64,commandLineOptions[i].integer64Option.max);
            }
            (void)fputc(')',outputHandle);
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
            (void)fputc(')',outputHandle);
          }
          break;
        case CMD_OPTION_TYPE_BOOLEAN:
          break;
        case CMD_OPTION_TYPE_FLAG:
          break;
        case CMD_OPTION_TYPE_INCREMENT:
          break;
        case CMD_OPTION_TYPE_ENUM:
          break;
        case CMD_OPTION_TYPE_SELECT:
          break;
        case CMD_OPTION_TYPE_SET:
          break;
        case CMD_OPTION_TYPE_CSTRING:
          break;
        case CMD_OPTION_TYPE_STRING:
          break;
        case CMD_OPTION_TYPE_SPECIAL:
          break;
        case CMD_OPTION_TYPE_DEPRECATED:
          break;
        case CMD_OPTION_TYPE_END:
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
        case CMD_OPTION_TYPE_FLAG:
          break;
        case CMD_OPTION_TYPE_INCREMENT:
          break;
        case CMD_OPTION_TYPE_ENUM:
          break;
        case CMD_OPTION_TYPE_SELECT:
          if (!stringIsEmpty(commandLineOptions[i].selectOption.descriptionDefaultText))
          {
            maxValueLength = 0;
            ITERATE_SELECT(select,commandLineOptions[i].selectOption.selects)
            {
              maxValueLength = MAX(strlen(select->name),maxValueLength);
            }

            ITERATE_SELECT(select,commandLineOptions[i].selectOption.selects)
            {
              printSpaces(outputHandle,strlen(PREFIX)+maxNameLength+((commandLineOptions[i].description != NULL)?2:0)+1);
              (void)fputs(select->name,outputHandle);
              printSpaces(outputHandle,maxValueLength-strlen(select->name));
              (void)fputs(": ",outputHandle);
              (void)fputs(select->description,outputHandle);
              if (select->value == commandLineOptions[i].defaultValue.select)
              {
                (void)fputc(' ',outputHandle);
                (void)fputs(commandLineOptions[i].selectOption.descriptionDefaultText,outputHandle);
              }
              (void)fputc('\n',outputHandle);
            }
          }
          break;
        case CMD_OPTION_TYPE_SET:
          if (!stringIsEmpty(commandLineOptions[i].setOption.descriptionDefaultText))
          {
            maxValueLength = 0;
            ITERATE_SET(set,commandLineOptions[i].setOption.sets)
            {
              maxValueLength = MAX(strlen(set->name),maxValueLength);
            }

            ITERATE_SET(set,commandLineOptions[i].setOption.sets)
            {
              printSpaces(outputHandle,strlen(PREFIX)+maxNameLength+((commandLineOptions[i].description != NULL)?2:0)+1);
              (void)fputs(set->name,outputHandle);
              printSpaces(outputHandle,maxValueLength-strlen(set->name));
              (void)fputs(": ",outputHandle);
              (void)fputs(set->description,outputHandle);
              if (set->value == commandLineOptions[i].defaultValue.set)
              {
                (void)fputc(' ',outputHandle);
                (void)fputs(commandLineOptions[i].setOption.descriptionDefaultText,outputHandle);
              }
              (void)fputc('\n',outputHandle);
            }
          }
          break;
        case CMD_OPTION_TYPE_CSTRING:
          break;
        case CMD_OPTION_TYPE_STRING:
          break;
        case CMD_OPTION_TYPE_SPECIAL:
          break;
        case CMD_OPTION_TYPE_DEPRECATED:
          break;
        case CMD_OPTION_TYPE_END:
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

#ifdef __GNUC__
#pragma GCC pop_options
#endif /* __GNUC__ */

#ifdef __GNUG__
}
#endif

/* end of file */
