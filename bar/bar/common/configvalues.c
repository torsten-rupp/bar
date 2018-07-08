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

#include "common/global.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"

#include "configvalues.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/

/***************************** Datatypes ******************************/

/***************************** Variables ******************************/

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

LOCAL const ConfigValueUnit *findUnit(const ConfigValueUnit *units, const char *unitName)
{
  const ConfigValueUnit *unit;

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
* Purpose: find unit by value
* Input  : units - units array
*          value - value
* Output : -
* Return : unit or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueUnit *findIntegerUnitByValue(const ConfigValueUnit *units, int value)
{
  const ConfigValueUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && (((int64)value % (int64)unit->factor) != 0)
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

LOCAL const ConfigValueUnit *findInteger64UnitByValue(const ConfigValueUnit *units, int64 value)
{
  const ConfigValueUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && ((value % (int64)unit->factor) != 0)
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
* Name   : findDoubleUnitByValue
* Purpose: find unit by name
* Input  : units - units array
*          value - value
* Output : -
* Return : unit or NULL if not found
* Notes  : -
\***********************************************************************/

#ifdef __GNUC__
  #pragma GCC push_options
  #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif /* __GNUC__ */

LOCAL const ConfigValueUnit *findDoubleUnitByValue(const ConfigValueUnit *units, double value)
{
  const ConfigValueUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && (fmod(value,unit->factor) != 0.0)
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

#pragma GCC pop_options

/***********************************************************************\
* Name   : findSelect
* Purpose: find select by name
* Input  : selects    - selects array
*          selectName - select name
* Output : -
* Return : select or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueSelect *findSelect(const ConfigValueSelect *selects, const char *selectName)
{
  const ConfigValueSelect *select;

  if (selects != NULL)
  {
    select = selects;
    while (   (select->name != NULL)
           && !stringEquals(select->name,selectName)
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

/***********************************************************************\
* Name   : findSelectByValue
* Purpose: find select by value
* Input  : selects - selects array
*          value   - value
* Output : -
* Return : select or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueSelect *findSelectByValue(const ConfigValueSelect *selects, uint value)
{
  const ConfigValueSelect *select;

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

/***********************************************************************\
* Name   : findSet
* Purpose: find set by name
* Input  : sets    - sets array
*          setName - set name
* Output : -
* Return : set or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueSet *findSet(const ConfigValueSet *sets, const char *setName)
{
  const ConfigValueSet *set;

  if (sets != NULL)
  {
    set = sets;
    while (   (set->name != NULL)
           && !stringEquals(set->name,setName)
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

LOCAL const ConfigValueSet *findSetByValue(const ConfigValueSet *sets, uint value)
{
  const ConfigValueSet *set;

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
#endif /* 0 */

/***********************************************************************\
* Name   : getIntegerOption
* Purpose: get integer value
* Input  : value         - value variable
*          string        - string
*          units         - units array or NULL
*          outputHandle  - error/warning output handle or NULL
*          errorPrefix   - error prefix or NULL
*          warningPrefix - warning prefix or NULL
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getIntegerValue(int                   *value,
                           const char            *string,
                           const ConfigValueUnit *units,
                           FILE                  *outputHandle,
                           const char            *errorPrefix,
                           const char            *warningPrefix
                          )
{
  uint                  i,j;
  char                  number[128],unitName[32];
  const ConfigValueUnit *unit;
  ulong                 factor;

  assert(value != NULL);
  assert(string != NULL);

  UNUSED_VARIABLE(warningPrefix);

  // split number, unit
  i = strlen(string);
  if (i > 0)
  {
    while ((i > 1) && !isdigit(string[i-1])) { i--; }
    j = MIN(i,               sizeof(number  )-1); strncpy(number,  &string[0],j); number  [j] = '\0';
    j = MIN(strlen(string)-i,sizeof(unitName)-1); strncpy(unitName,&string[i],j); unitName[j] = '\0';
  }
  else
  {
    number[0]   = '\0';
    unitName[0] = '\0';
  }

  // find factor
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
      if (outputHandle != NULL) fprintf(outputHandle,
                                        "%sUnexpected unit in value '%s'!\n",
                                        (errorPrefix != NULL) ? errorPrefix : "",
                                        string
                                       );
      return FALSE;
    }
  }
  else
  {
    factor = 1;
  }

  // calculate value
  (*value) = strtoll(number,NULL,0)*factor;

  return TRUE;
}

/***********************************************************************\
* Name   : getInteger64Value
* Purpose: get integer64 value
* Input  : value         - value variable
*          string        - string
*          units         - units array or NULL
*          outputHandle  - error/warning output handle or NULL
*          errorPrefix   - error prefix or NULL
*          warningPrefix - warning prefix or NULL
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getInteger64Value(int64                 *value,
                             const char            *string,
                             const ConfigValueUnit *units,
                             FILE                  *outputHandle,
                             const char            *errorPrefix,
                             const char            *warningPrefix
                            )
{
  uint                  i,j;
  char                  number[128],unitName[32];
  const ConfigValueUnit *unit;
  ulong                 factor;

  assert(value != NULL);
  assert(string != NULL);

  UNUSED_VARIABLE(warningPrefix);

  // split number, unit
  i = strlen(string);
  if (i > 0)
  {
    while ((i > 1) && !isdigit(string[i-1])) { i--; }
    j = MIN(i,               sizeof(number  )-1); strncpy(number,  &string[0],j); number  [j] = '\0';
    j = MIN(strlen(string)-i,sizeof(unitName)-1); strncpy(unitName,&string[i],j); unitName[j] = '\0';
  }
  else
  {
    number[0]   = '\0';
    unitName[0] = '\0';
  }

  // find factor
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
      if (outputHandle != NULL) fprintf(outputHandle,
                                        "%sUnexpected unit in value '%s'!\n",
                                        (errorPrefix != NULL) ? errorPrefix:"",
                                        string
                                       );
      return FALSE;
    }
  }
  else
  {
    factor = 1;
  }

  // calculate value
  (*value) = strtoll(number,NULL,0)*factor;

  return TRUE;
}

/***********************************************************************\
* Name   : processValue
* Purpose: process single config value
* Input  : configValue   - config value
*          prefix        - option prefix ("-" or "--")
*          name          - option name
*          value         - option value or NULL
*          outputHandle  - error/warning output handle or NULL
*          errorPrefix   - error prefix or NULL
*          warningPrefix - warning prefix or NULL
* Output : variable - variable to store value
* Return : TRUE if option processed without error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool processValue(const ConfigValue *configValue,
                        const char        *name,
                        const char        *value,
                        FILE              *outputHandle,
                        const char        *errorPrefix,
                        const char        *warningPrefix,
                        void              *variable
                       )
{
  union
  {
    void       *pointer;
    void       **reference;
    int        *i;
    int64      *l;
    double     *d;
    bool       *b;
    uint       *enumeration;
    uint       *select;
    ulong      *set;
    char       **cString;
    String     *string;
    void       *special;
    const char *newName;
  }    configVariable;
  char errorMessage[256];

  assert(configValue != NULL);
  assert(name != NULL);

  switch (configValue->type)
  {
    case CONFIG_VALUE_TYPE_NONE:
      break;
    case CONFIG_VALUE_TYPE_INTEGER:
      {
        int data;

        // get integer option value
        if (!getIntegerValue(&data,
                             value,
                             configValue->integerValue.units,
                             outputHandle,
                             errorPrefix,
                             warningPrefix
                            )
           )
        {
          return FALSE;
        }

        // check range
        if (   (data < configValue->integerValue.min)
            || (data > configValue->integerValue.max)
           )
        {
          if (outputHandle != NULL) fprintf(outputHandle,
                                            "%sValue '%s' out range %d..%d for config value '%s'!\n",
                                            (errorPrefix != NULL) ? errorPrefix:"",
                                            value,
                                            configValue->integerValue.min,
                                            configValue->integerValue.max,
                                            name
                                           );
          return FALSE;
        }

        // store value
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.i = (int*)((byte*)variable+configValue->offset);
            (*configVariable.i) = data;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.i = (int*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.i) = data;
            }
          }
        }
        else
        {
          assert(configValue->variable.i != NULL);
          (*configValue->variable.i) = data;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_INTEGER64:
      {
        int64 data;

        // get integer option value
        if (!getInteger64Value(&data,
                               value,
                               configValue->integer64Value.units,
                               outputHandle,
                               errorPrefix,
                               warningPrefix
                              )
           )
        {
          return FALSE;
        }

        // check range
        if (   (data < configValue->integer64Value.min)
            || (data > configValue->integer64Value.max)
           )
        {
          if (outputHandle != NULL) fprintf(outputHandle,
                                            "%sValue '%s' out range %lld..%lld for config value '%s'!\n",
                                            (errorPrefix != NULL) ? errorPrefix:"",
                                            value,
                                            configValue->integer64Value.min,
                                            configValue->integer64Value.max,
                                            name
                                           );
          return FALSE;
        }

        // store value
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.l = (int64*)((byte*)variable+configValue->offset);
            (*configVariable.l) = data;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.l = (int64*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.l) = data;
            }
          }
        }
        else
        {
          assert(configValue->variable.l != NULL);
          (*configValue->variable.l) = data;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_DOUBLE:
      {
        uint                  i,j;
        char                  number[128],unitName[32];
        const ConfigValueUnit *unit;
        ulong                 factor;
        double                data;

        // split number, unit
        i = strlen(value);
        if (i > 0)
        {
          while ((i > 1) && !isdigit(value[i-1])) { i--; }
          j = MIN(i,              sizeof(number  )-1); strncpy(number,  &value[0],j);number  [j] = '\0';
          j = MIN(strlen(value)-i,sizeof(unitName)-1); strncpy(unitName,&value[i],j);unitName[j] = '\0';
        }
        else
        {
          number[0]   = '\0';
          unitName[0] = '\0';
        }

        // find factor
        if (unitName[0] != '\0')
        {
          if (configValue->doubleValue.units != NULL)
          {
            unit = findUnit(configValue->doubleValue.units,unitName);
            if (unit == NULL)
            {
              if (outputHandle != NULL)
              {
                fprintf(outputHandle,
                        "%sInvalid unit in float value '%s'! Valid units:",
                        (errorPrefix != NULL) ? errorPrefix:"",
                        value
                       );
                ITERATE_UNITS(unit,configValue->doubleValue.units)
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
            if (outputHandle != NULL) fprintf(outputHandle,
                                              "%sUnexpected unit in value '%s'!\n",
                                              (errorPrefix != NULL) ? errorPrefix:"",
                                              value
                                             );
            return FALSE;
          }
        }
        else
        {
          factor = 1;
        }

        // calculate value
        data = strtod(value,0)*factor;
        if (   (data < configValue->doubleValue.min)
            || (data > configValue->doubleValue.max)
           )
        {
          if (outputHandle != NULL) fprintf(outputHandle,
                                            "%sValue '%s' out range %lf..%lf for float config value '%s'!\n",
                                            (errorPrefix != NULL) ? errorPrefix:"",
                                            value,
                                            configValue->doubleValue.min,
                                            configValue->doubleValue.max,
                                            name
                                           );
          return FALSE;
        }

        // store value
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.d = (double*)((byte*)variable+configValue->offset);
            (*configVariable.d) = data;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.d = (double*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.d) = data;
            }
          }
        }
        else
        {
          assert(configValue->variable.d != NULL);
          (*configValue->variable.d) = data;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_BOOLEAN:
      {
        bool data;

        // calculate value
        if      (   (value == NULL)
                 || stringEquals(value,"1")
                 || stringEqualsIgnoreCase(value,"true")
                 || stringEqualsIgnoreCase(value,"on")
                 || stringEqualsIgnoreCase(value,"yes")
                )
        {
          data = TRUE;
        }
        else if (   stringEquals(value,"0")
                 || stringEqualsIgnoreCase(value,"false")
                 || stringEqualsIgnoreCase(value,"off")
                 || stringEqualsIgnoreCase(value,"no")
                )
        {
          data = FALSE;
        }
        else
        {
          if (outputHandle != NULL) fprintf(outputHandle,
                                            "%sInvalid value '%s' for boolean config value '%s'!\n",
                                            (errorPrefix != NULL) ? errorPrefix:"",
                                            value,
                                            name
                                           );
          return FALSE;
        }

        // store value
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.b = (bool*)((byte*)variable+configValue->offset);
            (*configVariable.b) = data;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.b = (bool*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.b) = data;
            }
          }
        }
        else
        {
          assert(configValue->variable.b != NULL);
          (*configValue->variable.b) = data;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_ENUM:
      {
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.enumeration = (uint*)((byte*)variable+configValue->offset);
            (*configVariable.enumeration) = configValue->enumValue.enumerationValue;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.enumeration = (uint*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.enumeration) = configValue->enumValue.enumerationValue;
            }
          }
        }
        else
        {
          assert(configValue->variable.enumeration != NULL);
          (*configValue->variable.enumeration) = configValue->enumValue.enumerationValue;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_SELECT:
      {
        const ConfigValueSelect *select;

        // find select
        select = findSelect(configValue->selectValue.selects,value);
        if (select == NULL)
        {
          if (outputHandle != NULL) fprintf(outputHandle,
                                            "%sUnknown value '%s' for config value '%s'!\n",
                                            (errorPrefix != NULL) ? errorPrefix:"",
                                            value,
                                            name
                                           );
          return FALSE;
        }

        // store value
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.select = (uint*)((byte*)variable+configValue->offset);
            (*configVariable.select) = select->value;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.select = (uint*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.select) = select->value;
            }
          }
        }
        else
        {
          assert(configValue->variable.select != NULL);
          (*configValue->variable.select) = select->value;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_SET:
      {
        ulong                set;
        uint                 i,j;
        char                 setName[128];
        const ConfigValueSet *configValueSet;

        // find and store set values
        assert(configValue->variable.set != NULL);

        set = 0L;
        i = 0;
        while (value[i] != '\0')
        {
          // skip spaces
          while ((value[i] != '\0') && isspace(value[i])) { i++; }

          // get name
          j = 0;
          while ((value[i] != '\0') && (value[i] != ',') && !isspace(value[i]))
          {
            if (j < sizeof(setName)-1) { setName[j] = value[i]; j++; }
            i++;
          }
          setName[j] = '\0';

          // skip ,
          if (value[i] == ',') i++;

          if (setName[0] != '\0')
          {
            // find value
            configValueSet = findSet(configValue->setValue.sets,setName);
            if (configValueSet == NULL)
            {
              if (outputHandle != NULL) fprintf(outputHandle,
                                                "%sUnknown value '%s' for config value '%s'!\n",
                                                (errorPrefix != NULL) ? errorPrefix:"",
                                                setName,
                                                name
                                               );
              return FALSE;
            }

            // store value
            if (configValue->offset >= 0)
            {
              if (variable != NULL)
              {
                configVariable.set = (ulong*)((byte*)variable+configValue->offset);
                set |= configValueSet->value;
              }
              else
              {
                assert(configValue->variable.reference != NULL);
                if ((*configValue->variable.reference) != NULL)
                {
                  configVariable.set = (ulong*)((byte*)(*configValue->variable.reference)+configValue->offset);
                  set |= configValueSet->value;
                }
              }
            }
            else
            {
              assert(configValue->variable.set != NULL);
              set |= configValueSet->value;
            }
          }
        }
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.set = (ulong*)((byte*)variable+configValue->offset);
            (*configVariable.set) = set;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.set = (ulong*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.set) = set;
            }
          }
        }
        else
        {
          assert(configValue->variable.set != NULL);
          (*configValue->variable.set) = set;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_CSTRING:
      {
        if ((*configValue->variable.cString) != NULL)
        {
          free(*configValue->variable.cString);
        }

        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.cString = (char**)((byte*)variable+configValue->offset);
            (*configVariable.cString) = strdup(value);
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.cString = (char**)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.cString) = strdup(value);
            }
          }
        }
        else
        {
          assert(configValue->variable.cString != NULL);
          (*configValue->variable.cString) = strdup(value);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_STRING:
      {
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.string = (String*)((byte*)variable+configValue->offset);
            if ((*configVariable.string) == NULL) (*configVariable.string) = String_new();
            assert((*configVariable.string) != NULL);
            String_setCString(*configVariable.string,value);
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.string = (String*)((byte*)(*configValue->variable.reference)+configValue->offset);
              if ((*configVariable.string) == NULL) (*configVariable.string) = String_new();
              assert((*configVariable.string) != NULL);
              String_setCString(*configVariable.string,value);
            }
          }
        }
        else
        {
          assert(configValue->variable.string != NULL);
          if ((*configValue->variable.string) == NULL) (*configValue->variable.string) = String_new();
          assert((*configValue->variable.string) != NULL);
          String_setCString(*configValue->variable.string,value);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_SPECIAL:
      // store value
      if (configValue->offset >= 0)
      {
        if (variable != NULL)
        {
          errorMessage[0] = '\0';
          if (!configValue->specialValue.parse(configValue->specialValue.userData,
                                               (byte*)variable+configValue->offset,
                                               configValue->name,
                                               value,
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
                        "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                        (errorPrefix != NULL) ? errorPrefix:"",
                        value,
                        configValue->name,
                        errorMessage
                       );
              }
              else
              {
                fprintf(outputHandle,
                        "%sInvalid value '%s' for config value '%s'!\n",
                        (errorPrefix != NULL) ? errorPrefix:"",
                        value,
                        configValue->name
                       );
              }
            }
            return FALSE;
          }
        }
        else
        {
          assert(configValue->variable.reference != NULL);

          if ((*configValue->variable.reference) != NULL)
          {
            errorMessage[0] = '\0';
            if (!configValue->specialValue.parse(configValue->specialValue.userData,
                                                 (byte*)(*configValue->variable.reference)+configValue->offset,
                                                 configValue->name,
                                                 value,
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
                          "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                          (errorPrefix != NULL) ? errorPrefix:"",
                          value,
                          configValue->name,
                          errorMessage
                         );
                }
                else
                {
                  fprintf(outputHandle,
                          "%sInvalid value '%s' for config value '%s'!\n",
                          (errorPrefix != NULL) ? errorPrefix:"",
                          value,
                          configValue->name
                         );
                }
              }
              return FALSE;
            }
          }
        }
      }
      else
      {
        assert(configValue->variable.special != NULL);

        errorMessage[0] = '\0';
        if (!configValue->specialValue.parse(configValue->specialValue.userData,
                                             configValue->variable.special,
                                             configValue->name,
                                             value,
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
                      "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                      (errorPrefix != NULL) ? errorPrefix:"",
                      value,
                      configValue->name,
                      errorMessage
                     );
            }
            else
            {
              fprintf(outputHandle,
                      "%sInvalid value '%s' for config value '%s'!\n",
                      (errorPrefix != NULL) ? errorPrefix:"",
                      value,
                      configValue->name
                     );
            }
          }
          return FALSE;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_IGNORE:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_DEPRECATED:
      // store value
      if (configValue->offset >= 0)
      {
        if (variable != NULL)
        {
          errorMessage[0] = '\0';
          if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                  (byte*)variable+configValue->offset,
                                                  configValue->name,
                                                  value,
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
                        "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                        (errorPrefix != NULL) ? errorPrefix:"",
                        value,
                        configValue->name,
                        errorMessage
                       );
              }
              else
              {
                fprintf(outputHandle,
                        "%sInvalid value '%s' for config value '%s'!\n",
                        (errorPrefix != NULL) ? errorPrefix:"",
                        value,
                        configValue->name
                       );
              }
            }
            return FALSE;
          }
        }
        else
        {
          assert(configValue->variable.reference != NULL);

          if ((*configValue->variable.reference) != NULL)
          {
            errorMessage[0] = '\0';
            if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                    (byte*)(*configValue->variable.reference)+configValue->offset,
                                                    configValue->name,
                                                    value,
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
                          "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                          (errorPrefix != NULL) ? errorPrefix:"",
                          value,
                          configValue->name,
                          errorMessage
                         );
                }
                else
                {
                  fprintf(outputHandle,
                          "%sInvalid value '%s' for config value '%s'!\n",
                          (errorPrefix != NULL) ? errorPrefix:"",
                          value,
                          configValue->name
                         );
                }
              }
              return FALSE;
            }
          }
        }
      }
      else
      {
        assert(configValue->variable.special != NULL);

        errorMessage[0] = '\0';
        if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                configValue->variable.deprecated,
                                                configValue->name,
                                                value,
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
                      "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                      (errorPrefix != NULL) ? errorPrefix:"",
                      value,
                      configValue->name,
                      errorMessage
                     );
            }
            else
            {
              fprintf(outputHandle,
                      "%sInvalid value '%s' for config value '%s'!\n",
                      (errorPrefix != NULL) ? errorPrefix:"",
                      value,
                      configValue->name
                     );
            }
          }
          return FALSE;
        }
      }
      if ((outputHandle != NULL) && configValue->deprecatedValue.warningFlag)
      {
        fprintf(outputHandle,
                "%sConfiguration value '%s' is deprecated!",
                (warningPrefix != NULL) ? warningPrefix:"",
                configValue->name
               );
        if (configValue->deprecatedValue.newName != NULL)
        {
          fprintf(outputHandle,
                  " Use '%s' instead.",
                  configValue->deprecatedValue.newName
                 );
        }
        fprintf(outputHandle,"\n");
      }
      break;
    case CONFIG_VALUE_TYPE_BEGIN_SECTION:
    case CONFIG_VALUE_TYPE_END_SECTION:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_COMMENT:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_END:
      // nothing to do
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  return TRUE;
}

// ----------------------------------------------------------------------

bool ConfigValue_init(ConfigValue configValues[])
{
  assert(configValues != NULL);

  UNUSED_VARIABLE(configValues);

  return TRUE;
}

void ConfigValue_done(ConfigValue configValues[])
{
  assert(configValues != NULL);

  UNUSED_VARIABLE(configValues);
}

int ConfigValue_valueIndex(const ConfigValue configValues[],
                           const char        *sectionName,
                           const char        *name
                          )
{
  int index;

  assert(configValues != NULL);
  assert(name != NULL);

  index = ConfigValue_firstValueIndex(configValues,sectionName);
  while (   (index >= 0)
         && (configValues[index].type != CONFIG_VALUE_TYPE_END)
         && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
         && !stringEquals(configValues[index].name,name)
        )
  {
    index = ConfigValue_nextValueIndex(configValues,index);
  }

  return (   (index >= 0)
          && (configValues[index].type != CONFIG_VALUE_TYPE_END)
          && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
         )
           ? index
           : -1;
}

int ConfigValue_firstValueIndex(const ConfigValue configValues[],
                                const char        *sectionName
                               )
{
  uint index;
  bool skipFlag;

  assert(configValues != NULL);

  index = 0;
  if (sectionName != NULL)
  {
    while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
           && (   (configValues[index].type != CONFIG_VALUE_TYPE_BEGIN_SECTION)
               || !stringEquals(configValues[index].name,sectionName)
              )
          )
    {
      // skip section, comments
      do
      {
        skipFlag = TRUE;
        switch (configValues[index].type)
        {
          case  CONFIG_VALUE_TYPE_BEGIN_SECTION:
            do
            {
              index++;
            }
            while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
                   && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
                  );
            if (configValues[index].type == CONFIG_VALUE_TYPE_END_SECTION)
            {
              skipFlag = FALSE;
            }
            else
            {
              index++;
            }
            break;
          case  CONFIG_VALUE_TYPE_COMMENT:
            index++;
            break;
          case  CONFIG_VALUE_TYPE_END:
            skipFlag = FALSE;
            break;
          default:
            index++;
            skipFlag = FALSE;
            break;
        }
      }
      while (skipFlag);
    }
    if (configValues[index].type == CONFIG_VALUE_TYPE_BEGIN_SECTION) index++;
  }
  else
  {
    // skip sections, comments
    do
    {
      skipFlag = TRUE;
      switch (configValues[index].type)
      {
        case CONFIG_VALUE_TYPE_BEGIN_SECTION:
          do
          {
            index++;
          }
          while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
                 && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
                );
          if (configValues[index].type == CONFIG_VALUE_TYPE_END_SECTION)
          {
            skipFlag = FALSE;
          }
          else
          {
            index++;
          }
          break;
        case CONFIG_VALUE_TYPE_COMMENT:
          index++;
          break;
        case CONFIG_VALUE_TYPE_END:
        default:
          skipFlag = FALSE;
          break;
      }
    }
    while (skipFlag);
  }

  return (configValues[index].type != CONFIG_VALUE_TYPE_END) ? (int)index : -1;
}

int ConfigValue_lastValueIndex(const ConfigValue configValues[],
                               const char        *sectionName
                              )
{
  uint index;
  bool skipFlag;

  assert(configValues != NULL);

  index = 0;
  if (sectionName != NULL)
  {
    while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
           && (   (configValues[index].type != CONFIG_VALUE_TYPE_BEGIN_SECTION)
               || !stringEquals(configValues[index].name,sectionName)
              )
          )
    {
      // skip section, comments
      do
      {
        skipFlag = TRUE;
        switch (configValues[index].type)
        {
          case CONFIG_VALUE_TYPE_BEGIN_SECTION:
            do
            {
              index++;
            }
            while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
                   && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
                  );
            if (configValues[index].type == CONFIG_VALUE_TYPE_END_SECTION)
            {
              skipFlag = FALSE;
            }
            else
            {
              index++;
            }
            break;
          case CONFIG_VALUE_TYPE_COMMENT:
            index++;
            break;
          case CONFIG_VALUE_TYPE_END:
            skipFlag = FALSE;
            break;
          default:
            index++;
            skipFlag = FALSE;
            break;
        }
      }
      while (skipFlag);
    }

    while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
           && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
          )
    {
      index++;
    }
    index--;
  }
  else
  {
    while (configValues[index].type != CONFIG_VALUE_TYPE_END)
    {
      // skip sections, comments
      do
      {
        skipFlag = TRUE;
        switch (configValues[index].type)
        {
          case CONFIG_VALUE_TYPE_BEGIN_SECTION:
            do
            {
              index++;
            }
            while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
                   && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
                  );
            if (configValues[index].type == CONFIG_VALUE_TYPE_END_SECTION)
            {
              skipFlag = FALSE;
            }
            else
            {
              index++;
            }
            break;
          case CONFIG_VALUE_TYPE_COMMENT:
            index++;
            break;
          case CONFIG_VALUE_TYPE_END:
          default:
            skipFlag = FALSE;
            break;
        }
      }
      while (skipFlag);
      if (configValues[index].type != CONFIG_VALUE_TYPE_END) index++;
    }
    if (index > 0)
    {
      index--;

      // skip sections, comments
      do
      {
        skipFlag = TRUE;
        switch (configValues[index].type)
        {
          case CONFIG_VALUE_TYPE_BEGIN_SECTION:
            while ((index > 0) && (configValues[index].type == CONFIG_VALUE_TYPE_END_SECTION))
            {
              do
              {
                index--;
              }
              while (   (index > 0)
                     && (configValues[index].type != CONFIG_VALUE_TYPE_BEGIN_SECTION)
                    );
              if (configValues[index].type == CONFIG_VALUE_TYPE_BEGIN_SECTION)
              {
                if (index > 0) index--;
              }
            }
            skipFlag = FALSE;
            break;
          case CONFIG_VALUE_TYPE_COMMENT:
            index--;
            break;
          case CONFIG_VALUE_TYPE_END:
          default:
            skipFlag = FALSE;
            break;
        }
      }
      while (skipFlag);
    }
  }

  return (configValues[index].type != CONFIG_VALUE_TYPE_END) ? (int)index : -1;
}

int ConfigValue_nextValueIndex(const ConfigValue configValues[],
                               int               index
                              )
{
  bool skipFlag;

  assert(configValues != NULL);

  if ((index >= 0) && (configValues[index].type != CONFIG_VALUE_TYPE_END))
  {
    index++;

    // skip sections, comments
    do
    {
      skipFlag = TRUE;
      switch (configValues[index].type)
      {
        case CONFIG_VALUE_TYPE_BEGIN_SECTION:
          do
          {
            index++;
          }
          while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
                 && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
                );
          if (configValues[index].type == CONFIG_VALUE_TYPE_END_SECTION)
          {
            index++;
          }
          else
          {          
            skipFlag = FALSE;
          }
          break;
        case CONFIG_VALUE_TYPE_COMMENT:
          index++;
          break;
        case CONFIG_VALUE_TYPE_END:
        default:
          skipFlag = FALSE;
          break;
      }
    }
    while (skipFlag);
  }

  return (configValues[index].type != CONFIG_VALUE_TYPE_END) ? index : -1;
}

bool ConfigValue_parse(const char        *name,
                       const char        *value,
                       const ConfigValue configValues[],
                       const char        *sectionName,
                       FILE              *outputHandle,
                       const char        *errorPrefix,
                       const char        *warningPrefix,
                       void              *variable
                      )
{
  int i,j;

  assert(name != NULL);
  assert(configValues != NULL);

  // find config value
#if 0
  i = ConfigValue_firstValueIndex(configValues,sectionName);
  if (i < 0) return FALSE;
  j = ConfigValue_lastValueIndex(configValues,sectionName);
  if (j < 0) return FALSE;
  while (   (i <= j)
         && (   (configValues[i].name == NULL)
             || !stringEquals(configValues[i].name,name)
            )
        )
  {
    i = ConfigValue_nextValueIndex(configValues,i);
    if (i < 0) return FALSE;
  }
  if (i > j)
  {
    return FALSE;
  }
#else
  i = ConfigValue_valueIndex(configValues,sectionName,name);
  if (i < 0) return FALSE;
#endif

  // process value
  if (!processValue(&configValues[i],name,value,outputHandle,errorPrefix,warningPrefix,variable))
  {
    return FALSE;
  }

  return TRUE;
}

bool ConfigValue_getIntegerValue(int                   *value,
                                 const char            *string,
                                 const ConfigValueUnit *units
                                )
{
  return getIntegerValue(value,string,units,NULL,NULL,NULL);
}

bool ConfigValue_getInteger64Value(int64                 *value,
                                   const char            *string,
                                   const ConfigValueUnit *units
                                  )
{
  return getInteger64Value(value,string,units,NULL,NULL,NULL);
}

void ConfigValue_formatInit(ConfigValueFormat      *configValueFormat,
                            const ConfigValue      *configValue,
                            ConfigValueFormatModes mode,
                            const void             *variable
                           )
{
  assert(configValueFormat != NULL);
  assert(configValue != NULL);

  configValueFormat->formatUserData = NULL;
  configValueFormat->userData       = NULL;
  configValueFormat->configValue    = configValue;
  configValueFormat->mode           = mode;
  configValueFormat->endOfDataFlag  = FALSE;

  switch (configValue->type)
  {
    case CONFIG_VALUE_TYPE_NONE:
    case CONFIG_VALUE_TYPE_INTEGER:
    case CONFIG_VALUE_TYPE_INTEGER64:
    case CONFIG_VALUE_TYPE_DOUBLE:
    case CONFIG_VALUE_TYPE_BOOLEAN:
    case CONFIG_VALUE_TYPE_ENUM:
    case CONFIG_VALUE_TYPE_SELECT:
    case CONFIG_VALUE_TYPE_SET:
    case CONFIG_VALUE_TYPE_CSTRING:
    case CONFIG_VALUE_TYPE_STRING:
      configValueFormat->variable = variable;
      break;
    case CONFIG_VALUE_TYPE_SPECIAL:
      if (configValue->specialValue.formatInit != NULL)
      {
        if (configValueFormat->configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configValue->specialValue.formatInit(&configValueFormat->formatUserData,
                                                 configValue->specialValue.userData,
                                                 (byte*)variable+configValueFormat->configValue->offset
                                                );
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            if ((*configValueFormat->configValue->variable.reference) != NULL)
            {
              configValue->specialValue.formatInit(&configValueFormat->formatUserData,
                                                   configValue->specialValue.userData,
                                                   (byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset
                                                  );
            }
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.special != NULL);
          configValue->specialValue.formatInit(&configValueFormat->formatUserData,
                                               configValue->specialValue.userData,
                                               configValueFormat->configValue->variable.special
                                              );
        }
      }
      else
      {
        if (configValueFormat->configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configValueFormat->formatUserData = (byte*)variable+configValueFormat->configValue->offset;
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            if ((*configValueFormat->configValue->variable.reference) != NULL)
            {
              configValueFormat->formatUserData = (byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset;
            }
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.special != NULL);
          configValueFormat->formatUserData = configValueFormat->configValue->variable.special;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_IGNORE:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_DEPRECATED:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_BEGIN_SECTION:
    case CONFIG_VALUE_TYPE_END_SECTION:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_COMMENT:
//TODO
      break;
    case CONFIG_VALUE_TYPE_END:
      // nothing to do
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
}

void ConfigValue_formatDone(ConfigValueFormat *configValueFormat)
{
  assert(configValueFormat != NULL);
  assert(configValueFormat->configValue != NULL);

  switch (configValueFormat->configValue->type)
  {
    case CONFIG_VALUE_TYPE_NONE:
      break;
    case CONFIG_VALUE_TYPE_INTEGER:
    case CONFIG_VALUE_TYPE_INTEGER64:
    case CONFIG_VALUE_TYPE_DOUBLE:
    case CONFIG_VALUE_TYPE_BOOLEAN:
    case CONFIG_VALUE_TYPE_ENUM:
    case CONFIG_VALUE_TYPE_SELECT:
    case CONFIG_VALUE_TYPE_SET:
    case CONFIG_VALUE_TYPE_CSTRING:
    case CONFIG_VALUE_TYPE_STRING:
    case CONFIG_VALUE_TYPE_SPECIAL:
      if (configValueFormat->configValue->specialValue.formatDone != NULL)
      {
        configValueFormat->configValue->specialValue.formatDone(&configValueFormat->formatUserData,
                                                                configValueFormat->configValue->specialValue.userData
                                                               );
      }
      break;
    case CONFIG_VALUE_TYPE_IGNORE:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_DEPRECATED:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_BEGIN_SECTION:
    case CONFIG_VALUE_TYPE_END_SECTION:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_COMMENT:
//TODO
      break;
    case CONFIG_VALUE_TYPE_END:
      // nothing to do
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
}

#ifdef __GNUC__
  #pragma GCC push_options
  #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif /* __GNUC__ */

bool ConfigValue_format(ConfigValueFormat *configValueFormat,
                        String            line
                       )
{
  union
  {
    void       *pointer;
    void       **reference;
    int        *i;
    int64      *l;
    double     *d;
    bool       *b;
    uint       *enumeration;
    uint       *select;
    ulong      *set;
    char       **cString;
    String     *string;
    void       *special;
    const char *newName;
  }                       configVariable;
  const char              *unitName;
  const ConfigValueUnit   *unit;
  ulong                   factor;
  String                  s;
  const ConfigValueSelect *select;
  const ConfigValueSet    *set;

  assert(configValueFormat != NULL);
  assert(line != NULL);

  String_clear(line);
  if (!configValueFormat->endOfDataFlag)
  {
    switch (configValueFormat->mode)
    {
      case CONFIG_VALUE_FORMAT_MODE_VALUE:
        break;
      case CONFIG_VALUE_FORMAT_MODE_LINE:
        String_format(line,"%s = ",configValueFormat->configValue->name);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }

    switch (configValueFormat->configValue->type)
    {
      case CONFIG_VALUE_TYPE_NONE:
        break;
      case CONFIG_VALUE_TYPE_INTEGER:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.i = (int*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.i = (int*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.i != NULL);
          configVariable.i = configValueFormat->configValue->variable.i;
        }

        // find usable unit
        if ((configValueFormat->configValue->integerValue.units != NULL) && ((*configVariable.i) != 0))
        {
          unit = findIntegerUnitByValue(configValueFormat->configValue->integerValue.units,*configVariable.i);
          if (unit != NULL)
          {
            unitName = unit->name;
            factor   = unit->factor;
          }
          else
          {
            unitName = NULL;
            factor   = 0;
          }
        }
        else
        {
          unitName = NULL;
          factor   = 0;
        }

        if (factor > 0)
        {
          String_appendFormat(line,"%ld%s",(*configVariable.i)/factor,unitName);
        }
        else
        {
          String_appendFormat(line,"%ld",*configVariable.i);
        }

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_INTEGER64:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.l = (int64*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.l = (int64*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.l != NULL);
          configVariable.l = configValueFormat->configValue->variable.l;
        }

        // find usable unit
        if ((configValueFormat->configValue->integer64Value.units != NULL) && ((*configVariable.l) != 0L))
        {
          unit = findInteger64UnitByValue(configValueFormat->configValue->integer64Value.units,*configVariable.l);
          if (unit != NULL)
          {
            unitName = unit->name;
            factor   = unit->factor;
          }
          else
          {
            unitName = NULL;
            factor   = 0;
          }
        }
        else
        {
          unitName = NULL;
          factor   = 0;
        }

        if (factor > 0)
        {
          String_appendFormat(line,"%lld%s",(*configVariable.l)/factor,unitName);
        }
        else
        {
          String_appendFormat(line,"%lld",*configVariable.l);
        }

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_DOUBLE:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.d = (double*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.d = (double*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.d != NULL);
          configVariable.d = configValueFormat->configValue->variable.d;
        }

        // find usable unit
        if ((configValueFormat->configValue->doubleValue.units != NULL) && ((*configVariable.d) != 0.0))
        {
          unit = findDoubleUnitByValue(configValueFormat->configValue->doubleValue.units,*configVariable.d);
          if (unit != NULL)
          {
            unitName = unit->name;
            factor   = unit->factor;
          }
          else
          {
            unitName = NULL;
            factor   = 0;
          }
        }
        else
        {
          unitName = NULL;
          factor   = 0;
        }

        if (factor > 0)
        {
          String_appendFormat(line,"%lf",(*configVariable.d)/factor,unitName);
        }
        else
        {
          String_appendFormat(line,"%lf",*configVariable.d);
        }

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_BOOLEAN:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.b = (bool*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.b = (bool*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.b != NULL);
          configVariable.b = configValueFormat->configValue->variable.b;
        }

        // format value
        String_appendFormat(line,"%s",(*configVariable.b) ? "yes":"no");

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_ENUM:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.enumeration = (uint*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.enumeration = (uint*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.enumeration != NULL);
          configVariable.enumeration = configValueFormat->configValue->variable.enumeration;
        }

        // format value
        String_appendFormat(line,"%d",configVariable.enumeration);

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_SELECT:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.select = (uint*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.select = (uint*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.select != NULL);
          configVariable.select = configValueFormat->configValue->variable.select;
        }
        select = findSelectByValue(configValueFormat->configValue->selectValue.selects,*configVariable.select);

        // format value
        String_appendFormat(line,"%s",(select != NULL) ? select->name : "");

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_SET:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.set = (ulong*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.set = (ulong*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.set != NULL);
          configVariable.set = configValueFormat->configValue->variable.set;
        }
        s = String_new();
        ITERATE_SET(set,configValueFormat->configValue->setValue.sets)
        {
          if (((*configVariable.set) & set->value) == set->value)
          {
            if (String_length(s) > 0L) String_appendChar(s,',');
            String_appendCString(s,set->name);
          }
        }
        ITERATE_SET(set,configValueFormat->configValue->setValue.sets)
        {
          if ((*configVariable.set) == set->value)
          {
            String_setCString(s,set->name);
          }
        }

        // format value
        String_appendFormat(line,"%S",s);

        // free resources
        String_delete(s);

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_CSTRING:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.cString = (char**)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.cString = (char**)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.cString != NULL);
          configVariable.cString = configValueFormat->configValue->variable.cString;
        }

        // format value
        if (!stringIsEmpty(*configVariable.cString) && (stringFindChar(*configVariable.cString,' ') >= 0))
        {
          String_appendFormat(line,"%'s",*configVariable.cString);
        }
        else
        {
          String_appendFormat(line,"%s",*configVariable.cString);
        }

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_STRING:
        // get value
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.string = (String*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.string = (String*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.string != NULL);
          configVariable.string = configValueFormat->configValue->variable.string;
        }

        // format value
        String_appendFormat(line,"%'S",*configVariable.string);

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_SPECIAL:
#if 0
        if (configValueFormat->configValue->specialValue.format != NULL)
        {
          if (configValueFormat->configValue->offset >= 0)
          {
            if (configValueFormat->variable != NULL)
            {
              configValueFormat->endOfDataFlag = !configValueFormat->configValue->specialValue.format(&configValueFormat->formatUserData,
                                                                                                      configValueFormat->configValue->specialValue.userData,
                                                                                                      line
                                                                                                     );
            }
            else
            {
              assert(configValueFormat->configValue->variable.reference != NULL);
              if ((*configValueFormat->configValue->variable.reference) != NULL)
              {
                configValueFormat->endOfDataFlag = !configValueFormat->configValue->specialValue.format(&configValueFormat->formatUserData,
                                                                                                        configValueFormat->configValue->specialValue.userData,
                                                                                                        line
                                                                                                       );
              }
              else
              {
                configValueFormat->endOfDataFlag = TRUE;
              }
            }
          }
          else
          {
            assert(configValueFormat->configValue->variable.special != NULL);
            configValueFormat->endOfDataFlag = !configValueFormat->configValue->specialValue.format(&configValueFormat->formatUserData,
                                                                                                    configValueFormat->configValue->specialValue.userData,
                                                                                                    line
                                                                                                   );
          }
        }
        else
        {
          configValueFormat->endOfDataFlag = TRUE;
        }
        if (configValueFormat->endOfDataFlag) return FALSE;
#endif /* 0 */
        if (configValueFormat->configValue->specialValue.format != NULL)
        {
          configValueFormat->endOfDataFlag = !configValueFormat->configValue->specialValue.format(&configValueFormat->formatUserData,
                                                                                                  configValueFormat->configValue->specialValue.userData,
                                                                                                  line
                                                                                                 );
        }
        else
        {
          configValueFormat->endOfDataFlag = TRUE;
        }
        if (configValueFormat->endOfDataFlag) return FALSE;
        break;
      case CONFIG_VALUE_TYPE_IGNORE:
        // nothing to do
        return FALSE;
        break;
      case CONFIG_VALUE_TYPE_DEPRECATED:
        // nothing to do
        return FALSE;
        break;
      case CONFIG_VALUE_TYPE_BEGIN_SECTION:
      case CONFIG_VALUE_TYPE_END_SECTION:
        // nothing to do
        return FALSE;
        break;
      case CONFIG_VALUE_TYPE_COMMENT:
//TODO
        break;
      case CONFIG_VALUE_TYPE_END:
        // nothing to do
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *ConfigValue_selectToString(const ConfigValueSelect selects[],
                                       uint                    value,
                                       const char              *defaultString
                                      )
{
  const ConfigValueSelect *select;

  assert(selects != NULL);

  ITERATE_SELECT(select,selects)
  {
    if (select->value == value) return select->name;
  }

  return defaultString;
}

Errors ConfigValue_readConfigFileLines(ConstString configFileName, StringList *configLinesList)
{
  String     line;
  Errors     error;
  FileHandle fileHandle;

  assert(configFileName != NULL);
  assert(configLinesList != NULL);

  StringList_clear(configLinesList);

  // init variables
  line = String_new();

  // open file
  error = File_open(&fileHandle,configFileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // read file
  while (!File_eof(&fileHandle))
  {
    // read line
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE) break;

    StringList_append(configLinesList,line);
  }

  // close file
  File_close(&fileHandle);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // trim empty lines at begin/end
  while (!StringList_isEmpty(configLinesList) && String_isEmpty(StringList_first(configLinesList,NULL)))
  {
    StringList_remove(configLinesList,configLinesList->head);
  }
  while (!StringList_isEmpty(configLinesList) && String_isEmpty(StringList_last(configLinesList,NULL)))
  {
    StringList_remove(configLinesList,configLinesList->tail);
  }

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors ConfigValue_writeConfigFileLines(ConstString configFileName, const StringList *configLinesList)
{
  String     line;
  Errors     error;
  FileHandle fileHandle;
  StringNode *stringNode;

  assert(configFileName != NULL);
  assert(configLinesList != NULL);

  // open file
  error = File_open(&fileHandle,configFileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // write lines
  STRINGLIST_ITERATE(configLinesList,stringNode,line)
  {
    error = File_writeLine(&fileHandle,line);
    if (error != ERROR_NONE) break;
  }

  // close file
  File_close(&fileHandle);

  return error;
}

StringNode *ConfigValue_deleteEntries(StringList *stringList,
                                      const char *section,
                                      const char *name
                                     )
{
  StringNode *nextStringNode;

  StringNode *stringNode;
  String     line;
  String     string;

  nextStringNode = NULL;

  line       = String_new();
  string     = String_new();
  stringNode = stringList->head;
  while (stringNode != NULL)
  {
    // skip comments, empty lines
    String_trim(String_set(line,stringNode->string),STRING_WHITE_SPACES);
    if (String_isEmpty(line) || String_startsWithChar(line,'#'))
    {
      stringNode = stringNode->next;
      continue;
    }

    // parse and match
    if      (String_matchCString(line,STRING_BEGIN,"^\\s*\\[\\s*(\\S+).*\\]",NULL,STRING_NO_ASSIGN,string,NULL))
    {
      // keep line: begin section
      stringNode = stringNode->next;

      if (String_equalsCString(string,section))
      {
        // section found: remove matching entries
        while (stringNode != NULL)
        {
          // skip comments, empty lines
          String_trim(String_set(line,stringNode->string),STRING_WHITE_SPACES);
          if (String_isEmpty(line) || String_startsWithChar(line,'#'))
          {
            stringNode = stringNode->next;
            continue;
          }

          // parse and match
          if      (String_matchCString(line,STRING_BEGIN,"^\\s*\\[\\s*end\\s*]",NULL,NULL,NULL))
          {
            // keep line: end section
            stringNode = stringNode->next;
            break;
          }
          else if (   String_matchCString(line,STRING_BEGIN,"^(\\S+)\\s*=.*",NULL,STRING_NO_ASSIGN,string,NULL)
                   && String_equalsCString(string,name)
                  )
          {
            // delete line
            stringNode = StringList_remove(stringList,stringNode);

            // store next line
            nextStringNode = stringNode;
          }
          else
          {
            // keep line
            stringNode = stringNode->next;
          }
        }
      }
      else
      {
        // section not found: skip
        while (stringNode != NULL)
        {
          // skip comments, empty lines
          String_trim(String_set(line,stringNode->string),STRING_WHITE_SPACES);
          if (String_isEmpty(line) || String_startsWithChar(line,'#'))
          {
            stringNode = stringNode->next;
            continue;
          }

          // keep line
          stringNode = stringNode->next;

          // parse and match
          if (String_matchCString(line,STRING_BEGIN,"^\\s*\\[\\s*end\\s*]",NULL,NULL,NULL))
          {
            // end section
            break;
          }
        }
      }
    }
    else if (   String_matchCString(line,STRING_BEGIN,"^(\\S+)\\s*=.*",NULL,STRING_NO_ASSIGN,string,NULL)
             && String_equalsCString(string,name)
            )
    {
      // delete line
      stringNode = StringList_remove(stringList,stringNode);

      // store next line
      nextStringNode = stringNode;
    }
    else
    {
      // keep line
      stringNode = stringNode->next;
    }
  }
  String_delete(string);
  String_delete(line);

  return nextStringNode;
}

StringNode *ConfigValue_deleteSections(StringList *stringList,
                                       const char *section
                                      )
{
  StringNode *nextStringNode;

  StringNode *stringNode;
  String     line;
  String     string;

  nextStringNode = NULL;

  line       = String_new();
  string     = String_new();
  stringNode = stringList->head;
  while (stringNode != NULL)
  {
    // skip comments, empty lines
    String_trim(String_set(line,stringNode->string),STRING_WHITE_SPACES);
    if (String_isEmpty(line) || String_startsWithChar(line,'#'))
    {
      stringNode = stringNode->next;
      continue;
    }

    // parse and match
    if (   String_matchCString(line,STRING_BEGIN,"^\\s*\\[\\s*(\\S+).*\\]",NULL,STRING_NO_ASSIGN,string,NULL)
        && String_equalsCString(string,section)
       )
    {
      // delete section
      stringNode = StringList_remove(stringList,stringNode);
      while (   (stringNode != NULL)
             && !String_matchCString(stringNode->string,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
            )
      {
        stringNode = StringList_remove(stringList,stringNode);
      }
      if (stringNode != NULL)
      {
        if (String_matchCString(stringNode->string,STRING_BEGIN,"^\\s*\\[\\s*end\\s*]",NULL,NULL,NULL))
        {
          stringNode = StringList_remove(stringList,stringNode);
        }
      }

      // delete following empty lines
      while (   (stringNode != NULL)
             && String_isEmpty(String_trim(String_set(line,stringNode->string),STRING_WHITE_SPACES))
            )
      {
        stringNode = StringList_remove(stringList,stringNode);
      }

      // store next line
      nextStringNode = stringNode;
    }
    else
    {
      // keep line
      stringNode = stringNode->next;
    }
  }
  if (nextStringNode != NULL)
  {
    // delete previous empty lines
    while (   (nextStringNode->prev != NULL)
           && String_isEmpty(String_trim(String_set(line,nextStringNode->prev->string),STRING_WHITE_SPACES))
          )
    {
      StringList_remove(stringList,nextStringNode->prev);
    }
  }
  else
  {
    // delete empty lines at end
    while (!StringList_isEmpty(stringList) && String_isEmpty(StringList_last(stringList,NULL)))
    {
      StringList_remove(stringList,stringList->tail);
    }
  }
  String_delete(string);
  String_delete(line);

  return nextStringNode;
}

#ifdef __GNUC__
  #pragma GCC pop_options
#endif /* __GNUC__ */

#ifdef __GNUG__
}
#endif

/* end of file */
