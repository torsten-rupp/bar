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
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"

#include "configvalues.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/
#define SEPARATOR "----------------------------------------------------------------------"

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

/***********************************************************************\
* Name   : ITERATE_VALUE
* Purpose: iterated over config value array
* Input  : configValues - config values array
*          sectionName  - section name or NULL
*          index        - iteration variable
* Output : configValue - config value
* Return : -
* Notes  : configValue will contain all values
*          usage:
*            ITERATE_VALUE(configValues,index,5,10)
*            {
*              ... = configValues[index]...
*            }
\***********************************************************************/

#define ITERATE_VALUE(configValues,index,firstValueIndex,lastValueIndex) \
  for ((index) = firstValueIndex; \
       ((index) != CONFIG_VALUE_INDEX_NONE) && ((index) <= lastValueIndex); \
       (index)++ \
      )

/***********************************************************************\
* Name   : ITERATE_VALUEX
* Purpose: iterated over config value array
* Input  : configValues - config values array
*          sectionName  - section name or NULL
*          index        - iteration variable
*          condition    - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            ITERATE_VALUEX(configValues,index,5,10,TRUE)
*            {
*              ... = configValues[index]...
*            }
\***********************************************************************/

#define ITERATE_VALUEX(configValues,index,firstValueIndex,lastValueIndex,condition) \
  for ((index) = firstValueIndex; \
       ((index) != CONFIG_VALUE_INDEX_NONE) && ((index) <= lastValueIndex) && (condition); \
       (index)++ \
      )

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************\
* Name   : reportMessage
* Purpose: report message
* Input  : reportFunction - report function (can be NULL)
*          reportUserData - report user data
*          format         - format string (like printf)
*          ...            - optional arguments
* Output : -
* Return : unit or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL void reportMessage(ConfigReportFunction reportFunction,
                         void                 *reportUserData,
                         const char           *format,
                         ...
                        )
{
  va_list arguments;
  char    buffer[256];

  if (reportFunction != NULL)
  {
    va_start(arguments,format);
    stringVFormat(buffer,sizeof(buffer),format,arguments);
    va_end(arguments);

    reportFunction(buffer,reportUserData);
  }
}

/***********************************************************************\
* Name   : findUnitByName
* Purpose: find unit
* Input  : units    - units array
*          unitName - unit name
* Output : -
* Return : unit or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueUnit *findUnitByName(const ConfigValueUnit *units, const char *unitName)
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
* Name   : findMatchingUnit
* Purpose: find matching unit
* Input  : units - units array
*          value - value
* Output : -
* Return : unit or NULL if no matching unit found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueUnit *findMatchingUnit(const ConfigValueUnit *units, int value)
{
  const ConfigValueUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && (   ((uint)abs(value) < unit->factor)
               || ((value % unit->factor) != 0)
              )
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
* Name   : findMatchingUnit64
* Purpose: find matching unit
* Input  : units - units array
*          value - value
* Output : -
* Return : unit or NULL if no matching unit found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueUnit *findMatchingUnit64(const ConfigValueUnit *units, int64 value)
{
  const ConfigValueUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && (   ((uint64)llabs(value) < unit->factor)
               || ((value % unit->factor) != 0)
              )
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
* Name   : findMatchingUnitDouble
* Purpose: find matching unit
* Input  : units - units array
*          value - value
* Output : -
* Return : unit or NULL if no matching unit found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueUnit *findMatchingUnitDouble(const ConfigValueUnit *units, double value)
{
  const ConfigValueUnit *unit;

  if (units != NULL)
  {
    unit = units;
    while (   (unit->name != NULL)
           && (   (fabs(value) < unit->factor)
               || (fabs(fmod(value,unit->factor)) > 0.0)
              )
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
* Name   : findSelectByName
* Purpose: find select by name
* Input  : selects    - selects array
*          selectName - select name
* Output : -
* Return : select or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL const ConfigValueSelect *findSelectByName(const ConfigValueSelect *selects, const char *selectName)
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
* Name   : getIntegerValue
* Purpose: get integer value
* Input  : value                 - value variable
*          string                - string
*          units                 - units array or NULL
*          errorReportFunction   - error report function (can be NULL)
*          errorReportUserData   - error report user data
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getIntegerValue(int                   *value,
                           const char            *string,
                           const ConfigValueUnit *units,
                           ConfigReportFunction  errorReportFunction,
                           void                  *errorReportUserData
                          )
{
  uint                  i,j;
  char                  number[128],unitName[32];
  const ConfigValueUnit *unit;
  char                  buffer[256];
  ulong                 factor;

  assert(value != NULL);
  assert(string != NULL);

  // split number, unit
  i = stringLength(string);
  if (i > 0)
  {
    while ((i > 1) && !isdigit(string[i-1])) { i--; }
    j = MIN(i,                     sizeof(number  )-1); strncpy(number,  &string[0],j); number  [j] = '\0';
    j = MIN(stringLength(string)-i,sizeof(unitName)-1); strncpy(unitName,&string[i],j); unitName[j] = '\0';
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
      unit = findUnitByName(units,unitName);
      if (unit == NULL)
      {
        stringClear(buffer);
        ITERATE_UNITS(unit,units)
        {
          stringFormatAppend(buffer,sizeof(buffer)," %s",unit->name);
        }
        reportMessage(errorReportFunction,
                      errorReportUserData,
                      "Invalid unit in integer value '%s'! Valid units: %s",
                      string,
                      buffer
                     );
        return FALSE;
      }
      factor = unit->factor;
    }
    else
    {
      reportMessage(errorReportFunction,
                    errorReportUserData,
                    "Unexpected unit in value '%s'",
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
* Input  : value                 - value variable
*          string                - string
*          units                 - units array or NULL
*          errorReportFunction   - error report function (can be NULL)
*          errorReportUserData   - error report user data
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getInteger64Value(int64                 *value,
                             const char            *string,
                             const ConfigValueUnit *units,
                             ConfigReportFunction  errorReportFunction,
                             void                  *errorReportUserData
                            )
{
  uint                  i,j;
  char                  number[128],unitName[32];
  const ConfigValueUnit *unit;
  char                  buffer[256];
  ulong                 factor;

  assert(value != NULL);
  assert(string != NULL);

  // split number, unit
  i = stringLength(string);
  if (i > 0)
  {
    while ((i > 1) && !isdigit(string[i-1])) { i--; }
    j = MIN(i,                     sizeof(number  )-1); strncpy(number,  &string[0],j); number  [j] = '\0';
    j = MIN(stringLength(string)-i,sizeof(unitName)-1); strncpy(unitName,&string[i],j); unitName[j] = '\0';
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
      unit = findUnitByName(units,unitName);
      if (unit == NULL)
      {
        stringClear(buffer);
        ITERATE_UNITS(unit,units)
        {
          stringFormatAppend(buffer,sizeof(buffer)," %s",unit->name);
        }
        reportMessage(errorReportFunction,
                      errorReportUserData,
                      "Invalid unit in integer value '%s'! Valid units: %s",
                      string,
                      buffer
                     );
        return FALSE;
      }
      factor = unit->factor;
    }
    else
    {
      reportMessage(errorReportFunction,
                    errorReportUserData,
                    "Unexpected unit in value '%s'",
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
* Input  : configValue           - config value
*          sectionName           - section name or NULL
*          value                 - option value or NULL
*          errorReportFunction   - error report function (can be NULL)
*          errorReportUserData   - error report user data
*          warningReportFunction - warning report function (can be NULL)
*          warningReportUserData - warning report user data
* Output : variable - variable to store value
* Return : TRUE if option processed without error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool processValue(const ConfigValue    *configValue,
                        const char           *sectionName,
                        const char           *value,
                        ConfigReportFunction errorReportFunction,
                        void                 *errorReportUserData,
                        ConfigReportFunction warningReportFunction,
                        void                 *warningReportUserData,
                        void                 *variable
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
  }          configVariable;
  char       buffer[256];
  char       errorMessage[256];
  const char *message;

  assert(configValue != NULL);

  stringClear(errorMessage);
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
                             errorReportFunction,
                             errorReportUserData
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
          reportMessage(errorReportFunction,
                        errorReportUserData,
                        "Value '%s' out range %d..%d for config value '%s'",
                        value,
                        configValue->integerValue.min,
                        configValue->integerValue.max,
                        configValue->name
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
                               errorReportFunction,
                               errorReportUserData
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
          reportMessage(errorReportFunction,
                        errorReportUserData,
                        "Value '%s' out range %"PRIi64"..%"PRIi64" for config value '%s'",
                        value,
                        configValue->integer64Value.min,
                        configValue->integer64Value.max,
                        configValue->name
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
        i = stringLength(value);
        if (i > 0)
        {
          while ((i > 1) && !isdigit(value[i-1])) { i--; }
          j = MIN(i,                    sizeof(number  )-1); strncpy(number,  &value[0],j);number  [j] = '\0';
          j = MIN(stringLength(value)-i,sizeof(unitName)-1); strncpy(unitName,&value[i],j);unitName[j] = '\0';
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
            unit = findUnitByName(configValue->doubleValue.units,unitName);
            if (unit == NULL)
            {
              stringClear(buffer);
              ITERATE_UNITS(unit,configValue->doubleValue.units)
              {
                stringFormatAppend(buffer,sizeof(buffer)," %s",unit->name);
              }
              reportMessage(errorReportFunction,
                            errorReportUserData,
                            "Invalid unit in float value '%s'! Valid units: %s",
                            value,
                            buffer
                           );
              return FALSE;
            }
            factor = unit->factor;
          }
          else
          {
            reportMessage(errorReportFunction,
                          errorReportUserData,
                          "Unexpected unit in value '%s'",
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
          reportMessage(errorReportFunction,
                        errorReportUserData,
                        "Value '%s' out range %lf..%lf for float config value '%s'",
                        value,
                        configValue->doubleValue.min,
                        configValue->doubleValue.max,
                        configValue->name
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
          reportMessage(errorReportFunction,
                        errorReportUserData,
                        "Invalid value '%s' for boolean config value '%s'",
                        value,
                        configValue->name
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
        select = findSelectByName(configValue->selectValue.selects,value);
        if (select == NULL)
        {
          reportMessage(errorReportFunction,
                        errorReportUserData,
                        "Unknown value '%s' for config value '%s'",
                        value,
                        configValue->name
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
              reportMessage(errorReportFunction,
                            errorReportUserData,
                            "Unknown value '%s' for config value '%s'",
                            setName,
                            configValue->name
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
        String string;

        // unquote/unescape
        string = String_newCString(value);
        String_unquote(string,STRING_QUOTES);
        String_unescape(string,
                        STRING_ESCAPE_CHARACTER,
                        STRING_ESCAPE_CHARACTERS_MAP_TO,
                        STRING_ESCAPE_CHARACTERS_MAP_FROM,
                        STRING_ESCAPE_CHARACTER_MAP_LENGTH
                      );

        // free old string
        if ((*configValue->variable.cString) != NULL)
        {
          free(*configValue->variable.cString);
        }

        // store string
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.cString = (char**)((byte*)variable+configValue->offset);
            (*configVariable.cString) = stringNewBuffer(String_cString(string),String_length(string)+1);
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.cString = (char**)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.cString) = stringNewBuffer(String_cString(string),String_length(string)+1);
            }
          }
        }
        else
        {
          assert(configValue->variable.cString != NULL);
          (*configValue->variable.cString) = stringNewBuffer(String_cString(string),String_length(string)+1);
        }

        // free resources
        String_delete(string);
      }
      break;
    case CONFIG_VALUE_TYPE_STRING:
      {
        String string;

        // unquote/unescape
        string = String_newCString(value);
        String_unquote(string,STRING_QUOTES);
        String_unescape(string,
                        STRING_ESCAPE_CHARACTER,
                        STRING_ESCAPE_CHARACTERS_MAP_TO,
                        STRING_ESCAPE_CHARACTERS_MAP_FROM,
                        STRING_ESCAPE_CHARACTER_MAP_LENGTH
                      );

        // store string
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.string = (String*)((byte*)variable+configValue->offset);
            if ((*configVariable.string) == NULL) (*configVariable.string) = String_new();
            assert((*configVariable.string) != NULL);
            String_set(*configVariable.string,string);
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.string = (String*)((byte*)(*configValue->variable.reference)+configValue->offset);
              if ((*configVariable.string) == NULL) (*configVariable.string) = String_new();
              assert((*configVariable.string) != NULL);
              String_set(*configVariable.string,string);
            }
          }
        }
        else
        {
          assert(configValue->variable.string != NULL);
          if ((*configValue->variable.string) == NULL) (*configValue->variable.string) = String_new();
          assert((*configValue->variable.string) != NULL);
          String_set(*configValue->variable.string,string);
        }

        // free resources
        String_delete(string);
      }
      break;
    case CONFIG_VALUE_TYPE_SPECIAL:
      // store value
      if (configValue->offset >= 0)
      {
        if (variable != NULL)
        {
          if (!configValue->specialValue.parse(configValue->specialValue.userData,
                                               (byte*)variable+configValue->offset,
                                               configValue->name,
                                               value,
                                               errorMessage,
                                               sizeof(errorMessage)
                                              )
             )
          {
            if (!stringIsEmpty(errorMessage))
            {
              reportMessage(errorReportFunction,
                            errorReportUserData,
                            "%s for config value '%s'",
                            errorMessage,
                            configValue->name
                           );
            }
            else
            {
              reportMessage(errorReportFunction,
                            errorReportUserData,
                            "Invalid value '%s' for config value '%s'",
                            value,
                            configValue->name
                           );
            }
            return FALSE;
          }
        }
        else
        {
          assert(configValue->variable.reference != NULL);

          if ((*configValue->variable.reference) != NULL)
          {
            if (!configValue->specialValue.parse(configValue->specialValue.userData,
                                                 (byte*)(*configValue->variable.reference)+configValue->offset,
                                                 configValue->name,
                                                 value,
                                                 errorMessage,
                                                 sizeof(errorMessage)
                                                )
               )
            {
              if (!stringIsEmpty(errorMessage))
              {
                reportMessage(errorReportFunction,
                              errorReportUserData,
                              "%s for config value '%s'",
                              errorMessage,
                              configValue->name
                             );
              }
              else
              {
                reportMessage(errorReportFunction,
                              errorReportUserData,
                              "Invalid value '%s' for config value '%s'",
                              value,
                              configValue->name
                             );
              }
              return FALSE;
            }
          }
        }
      }
      else
      {
        assert(configValue->variable.special != NULL);

        if (!configValue->specialValue.parse(configValue->specialValue.userData,
                                             configValue->variable.special,
                                             configValue->name,
                                             value,
                                             errorMessage,
                                             sizeof(errorMessage)
                                            )
           )
        {
          if (!stringIsEmpty(errorMessage))
          {
            reportMessage(errorReportFunction,
                          errorReportUserData,
                          "%s for config value '%s'",
                          errorMessage,
                          configValue->name
                         );
          }
          else
          {
            reportMessage(errorReportFunction,
                          errorReportUserData,
                          "Invalid value '%s' for config value '%s'",
                          value,
                          configValue->name
                         );
          }
          return FALSE;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_IGNORE:
      if (configValue->ignoreValue.warningFlag)
      {
        if (sectionName != NULL)
        {
          if (configValue->deprecatedValue.newName != NULL)
          {
            reportMessage(warningReportFunction,
                          warningReportUserData,
                          "Configuration value '%s' in section '%s' is ignored. Use '%s' instead",
                          configValue->name,
                          sectionName,
                          configValue->deprecatedValue.newName
                         );
          }
          else
          {
            reportMessage(warningReportFunction,
                          warningReportUserData,
                          "Configuration value '%s' in section '%s' is ignored",
                          configValue->name,
                          sectionName
                         );
          }
        }
        else
        {
          if (configValue->deprecatedValue.newName != NULL)
          {
            reportMessage(warningReportFunction,
                          warningReportUserData,
                          "Configuration value '%s' is ignored. Use '%s' instead",
                          configValue->name,
                          configValue->deprecatedValue.newName
                         );
          }
          else
          {
            reportMessage(warningReportFunction,
                          warningReportUserData,
                          "Configuration value '%s' is ignored",
                          configValue->name
                         );
          }
        }
      }
      break;
    case CONFIG_VALUE_TYPE_DEPRECATED:
      // store value
      if (configValue->offset >= 0)
      {
        if (variable != NULL)
        {
          if (configValue->deprecatedValue.parse != NULL)
          {
            if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                    (byte*)variable+configValue->offset,
                                                    configValue->name,
                                                    value,
                                                    errorMessage,
                                                    sizeof(errorMessage)
                                                   )
               )
            {
              if (!stringIsEmpty(errorMessage))
              {
                reportMessage(errorReportFunction,
                              errorReportUserData,
                              "%s for config value '%s'",
                              errorMessage,
                              configValue->name
                             );
              }
              else
              {
                reportMessage(errorReportFunction,
                              errorReportUserData,
                              "Invalid value '%s' for config value '%s'",
                              value,
                              configValue->name
                             );
              }
              return FALSE;
            }
          }
        }
        else
        {
          if (configValue->variable.reference != NULL)
          {
            if (configValue->deprecatedValue.parse != NULL)
            {
              if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                      (byte*)(configValue->variable.reference)+configValue->offset,
                                                      configValue->name,
                                                      value,
                                                      errorMessage,
                                                      sizeof(errorMessage)
                                                     )
                 )
              {
                if (!stringIsEmpty(errorMessage))
                {
                  reportMessage(errorReportFunction,
                                errorReportUserData,
                                "%s for config value '%s'!",
                                errorMessage,
                                configValue->name
                               );
                }
                else
                {
                  reportMessage(errorReportFunction,
                                errorReportUserData,
                                "Invalid value '%s' for config value '%s'",
                                value,
                                configValue->name
                               );
                }
                return FALSE;
              }
            }
          }
        }
      }
      else
      {
        if (variable != NULL)
        {
          if (configValue->deprecatedValue.parse != NULL)
          {
            if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                    variable,
                                                    configValue->name,
                                                    value,
                                                    errorMessage,
                                                    sizeof(errorMessage)
                                                   )
               )
            {
              if (!stringIsEmpty(errorMessage))
              {
                reportMessage(errorReportFunction,
                              errorReportUserData,
                              "%s for config value '%s'",
                              errorMessage,
                              configValue->name
                             );
              }
              else
              {
                reportMessage(errorReportFunction,
                              errorReportUserData,
                              "Invalid value '%s' for config value '%s'",
                              value,
                              configValue->name
                             );
              }
              return FALSE;
            }
          }
        }
        else
        {
          assert((configValue->variable.deprecated != NULL) || (configValue->deprecatedValue.parse == NULL));

          if (configValue->deprecatedValue.parse != NULL)
          {
            if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                    configValue->variable.deprecated,
                                                    configValue->name,
                                                    value,
                                                    errorMessage,
                                                    sizeof(errorMessage)
                                                   )
               )
            {
              if (!stringIsEmpty(errorMessage))
              {
                reportMessage(errorReportFunction,
                              errorReportUserData,
                              "%s for config value '%s'",
                              errorMessage,
                              configValue->name
                             );
              }
              else
              {
                reportMessage(errorReportFunction,
                              errorReportUserData,
                              "Invalid value '%s' for config value '%s'",
                              value,
                              configValue->name
                             );
              }
              return FALSE;
            }
          }
        }
      }
      if (configValue->deprecatedValue.warningFlag)
      {
        if (sectionName != NULL)
        {
          if (configValue->deprecatedValue.newName != NULL)
          {
            message = (configValue->deprecatedValue.parse != NULL)
                        ? "Configuration value '%s' in section '%s' is deprecated. Use '%s' instead"
                        : "Configuration value '%s' in section '%s' is deprecated - skipped. Use '%s' instead";
            reportMessage(warningReportFunction,
                          warningReportUserData,
                          message,
                          configValue->name,
                          sectionName,
                          configValue->deprecatedValue.newName
                         );
          }
          else
          {
            message = (configValue->deprecatedValue.parse != NULL)
                        ? "Configuration value '%s' in section '%s' is deprecated"
                        : "Configuration value '%s' in section '%s' is deprecated - skipped";
            reportMessage(warningReportFunction,
                          warningReportUserData,
                          message,
                          configValue->name,
                          sectionName
                         );
          }
        }
        else
        {
          if (configValue->deprecatedValue.newName != NULL)
          {
            message = (configValue->deprecatedValue.parse != NULL)
                        ? "Configuration value '%s' is deprecated. Use '%s' instead"
                        : "Configuration value '%s' is deprecated - skipped. Use '%s' instead";
            reportMessage(warningReportFunction,
                          warningReportUserData,
                          message,
                          configValue->name,
                          configValue->deprecatedValue.newName
                         );
          }
          else
          {
            message = (configValue->deprecatedValue.parse != NULL)
                        ? "Configuration value '%s' is deprecated"
                        : "Configuration value '%s' is deprecated - skipped";
            reportMessage(warningReportFunction,
                          warningReportUserData,
                          message,
                          configValue->name
                         );
          }
        }
      }
      break;
    case CONFIG_VALUE_TYPE_BEGIN_SECTION:
    case CONFIG_VALUE_TYPE_END_SECTION:
      // nothing to do
      break;
    case CONFIG_VALUE_TYPE_SEPARATOR:
    case CONFIG_VALUE_TYPE_SPACE:
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

/***********************************************************************\
* Name   : writeCommentLines
* Purpose: write comment lines
* Input  : fileHandle         - file handle
*          indent             - indention
*          commentList        - comment lines list
*          defaultCommentList - default comment lines list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeCommentLines(FileHandle       *fileHandle,
                               uint             indent,
                               const StringList *commentList,
                               const StringList *defaultCommentList
                              )
{
  const StringList *stringList;
  Errors           error;
  StringNode       *iterator;
  String           string;

  assert(fileHandle != NULL);

  error = ERROR_NONE;

  stringList = ((commentList != NULL) && !StringList_isEmpty(commentList)) ? commentList : defaultCommentList;
  if (stringList != NULL)
  {
    STRINGLIST_ITERATEX(stringList,iterator,string,error == ERROR_NONE)
    {
      error = File_printLine(fileHandle,"%*C# %S",indent,' ',string);
    }
  }

  return error;
}

/***********************************************************************\
* Name   : flushCommentLines
* Purpose: flush comment lines
* Input  : fileHandle         - file handle
*          indent             - indention
*          commentList        - comment lines list
*          defaultCommentList - default comment lines list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors flushCommentLines(FileHandle *fileHandle,
                               uint       indent,
                               StringList *commentList
                              )
{
  Errors     error;

  assert(fileHandle != NULL);

  error = ERROR_NONE;
  if (commentList != NULL)
  {
    error = writeCommentLines(fileHandle,indent,commentList,NULL);
    StringList_clear(commentList);
  }

  return error;
}

/***********************************************************************\
* Name   : writeValue
* Purpose: write value
* Input  : fileHandle  - file handle
*          indent      - indention
*          configValue - config valule
*          variable    - variable
*          commentList - comment lines list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeValue(FileHandle        *fileHandle,
                        uint              indent,
                        const ConfigValue *configValue,
                        const void        *variable,
                        const StringList  *commentList
                       )
{
  Errors error;
  union
  {
    void        *pointer;
    void        **reference;
    int         *i;
    int64       *l;
    double      *d;
    bool        *b;
    uint        *enumeration;
    uint        *select;
    ulong       *set;
    const char  **cString;
    ConstString *string;
    void        *special;
    const char  *newName;
  }      configVariable;

  assert(fileHandle != NULL);
  assert(configValue != NULL);

  error = ERROR_NONE;
  switch (configValue->type)
  {
    case CONFIG_VALUE_TYPE_NONE:
      break;
    case CONFIG_VALUE_TYPE_INTEGER:
      {
        int                   value;
        const ConfigValueUnit *unit;

        // get value
        value = 0;
        if      (configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configVariable.i = (int*)((byte*)variable+configValue->offset);
            value = *configVariable.i;
          }
          else if (configValue->variable.reference != NULL)
          {
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.i = ((int*)((byte*)(*configValue->variable.reference)+configValue->offset));
              value = *configVariable.i;
            }
          }
        }
        else if (configValue->variable.i != NULL)
        {
          value = *configValue->variable.i;
        }

        // get unit
        unit = findMatchingUnit(configValue->integerValue.units,value);
        if (unit != NULL)
        {
          value = value/unit->factor;
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (value!=0)
        {
          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %d%s",indent,' ',configValue->name,value,(unit != NULL) ? unit->name : "");
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_INTEGER64:
      {
        int64                 value;
        const ConfigValueUnit *unit;

        // get value
        value = 0;
        if        (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.l = ((int64*)((byte*)variable+configValue->offset));
            value = *configVariable.l;
          }
          else if (configValue->variable.reference != NULL)
          {
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.l = ((int64*)((byte*)(*configValue->variable.reference)+configValue->offset));
              value = *configVariable.l;
            }
          }
        }
        else if (configValue->variable.l != NULL)
        {
          value = *configValue->variable.l;
        }

        // get unit
        unit = findMatchingUnit64(configValue->integer64Value.units,value);
        if (unit != NULL)
        {
          value = value/unit->factor;
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (value!=0)
        {
          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %"PRIi64"%s",indent,' ',configValue->name,value,(unit != NULL) ? unit->name : "");
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_DOUBLE:
      {
        double                value;
        const ConfigValueUnit *unit;

        // get value
        value = 0.0;
        if (configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configVariable.d = ((double*)((byte*)variable+configValue->offset));
            value = *configVariable.d;
          }
          else if (configValue->variable.reference != NULL)
          {
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.d = ((double*)((byte*)(*configValue->variable.reference)+configValue->offset));
              value = *configVariable.d;
            }
          }
        }
        else if (configValue->variable.d != NULL)
        {
          value = *configValue->variable.d;
        }

        // get unit
        unit = findMatchingUnitDouble(configValue->integer64Value.units,value);
        if (unit != NULL)
        {
          value = value/unit->factor;
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (value!=0)
        {
          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %"PRIi64"%s",indent,' ',configValue->name,value,(unit != NULL) ? unit->name : "");
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_BOOLEAN:
      {
        bool value;

        // get value
        value = FALSE;
        if      (configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configVariable.b = (bool*)((byte*)variable+configValue->offset);
            value = (*configVariable.b);
          }
          else if (configValue->variable.reference != NULL)
          {
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.b = (bool*)((byte*)(*configValue->variable.reference)+configValue->offset);
              value = (*configVariable.b);
            }
          }
        }
        else if (configValue->variable.b != NULL)
        {
          value = (*configValue->variable.b);
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (value)
        {
          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %s",indent,' ',configValue->name,value ? "yes" : "no");
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_ENUM:
#if 0
      {
        uint                    value;

        // get value
        value = 0;
        if      (configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configVariable.enumeration = (uint*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
            value = *configVariable.enumeration;
          }
          else if (configValue->variable.reference != NULL)
          {
            configVariable.enumeration = (uint*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
            value = *configVariable.enumeration;
          }
          else
          {
            return FALSE;
          }
        }
        else if (configValue->variable.enumeration != NULL)
        {
          configVariable.enumeration = configValueFormat->configValue->variable.enumeration;
          value = *configVariable.enumeration;
        }
        else
        {
          return FALSE;
        }

        // find select
        select = findSelectByValue(configValue->selectValue.selects,value);
        if (select == NULL)
        {
          return FALSE;
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (value!=0)
        {
          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %s",indent,' ',configValue->name,select->name);
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }
      }
#endif
      break;
    case CONFIG_VALUE_TYPE_SELECT:
      {
        uint                    value;
        const ConfigValueSelect *select;

        // get value
        value = 0;
        if      (configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configVariable.select = (uint*)((byte*)variable+configValue->offset);
            value = *configVariable.select;
          }
          else if (configValue->variable.reference != NULL)
          {
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.select = (uint*)((byte*)(*configValue->variable.reference)+configValue->offset);
              value = *configVariable.select;
            }
          }
        }
        else if (configValue->variable.select != NULL)
        {
          value = *configValue->variable.select;
        }

        // find select
        select = findSelectByValue(configValue->selectValue.selects,value);
        if (select == NULL)
        {
          return FALSE;
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (value!=0)
        {
          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %s",indent,' ',configValue->name,select->name);
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_SET:
      {
        ulong                value;
        String               s;
        const ConfigValueSet *configValueSet;

        // get value
        if      (configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configVariable.set = (ulong*)((byte*)variable+configValue->offset);
            value = *configVariable.set;
          }
          else if (configValue->variable.reference != NULL)
          {
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.set = (ulong*)((byte*)(*configValue->variable.reference)+configValue->offset);
              value = *configVariable.set;
            }
          }
        }
        else if (configValue->variable.set != NULL)
        {
          value = *configValue->variable.set;
        }
        else
        {
          return FALSE;
        }

        // format
        s = String_new();
        ITERATE_SET(configValueSet,configValue->setValue.sets)
        {
          if ((value & configValueSet->value) == configValueSet->value)
          {
            if (String_length(s) > 0L) String_appendChar(s,',');
            String_appendCString(s,configValueSet->name);
          }
        }
        ITERATE_SET(configValueSet,configValue->setValue.sets)
        {
          if (value == configValueSet->value)
          {
            String_setCString(s,configValueSet->name);
          }
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (!String_isEmpty(s))
        {
          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %s",indent,' ',configValue->name,String_cString(s));
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }

        // free resources
        String_delete(s);
      }
      break;
    case CONFIG_VALUE_TYPE_CSTRING:
      {
        String value;

        // init variables
        value = String_new();

        // get value
        if     (configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configVariable.cString = (const char**)((const byte*)variable+configValue->offset);
            String_setCString(value,*configVariable.cString);
          }
          else if (configValue->variable.reference != NULL)
          {
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.cString = (const char**)((const byte*)(*configValue->variable.reference)+configValue->offset);
              String_setCString(value,*configVariable.cString);
            }
          }
        }
        else if (configValue->variable.cString != NULL)
        {
          String_setCString(value,*configValue->variable.cString);
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (!String_isEmpty(value))
        {
          String_escape(value,
                        STRING_ESCAPE_CHARACTER,
                        NULL,  // chars
                        STRING_ESCAPE_CHARACTERS_MAP_FROM,
                        STRING_ESCAPE_CHARACTERS_MAP_TO,
                        STRING_ESCAPE_CHARACTER_MAP_LENGTH
                      );
          String_quote(value,STRING_QUOTE,NULL);

          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %S",indent,' ',configValue->name,value);
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }

        // free resources
        String_delete(value);
      }
      break;
    case CONFIG_VALUE_TYPE_STRING:
      {
        String value;

        // init variables
        value = String_new();

        // get value
        if      (configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configVariable.string = (ConstString*)((const byte*)variable+configValue->offset);
            String_set(value,*configVariable.string);
          }
          else if (configValue->variable.reference != NULL)
          {
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.string = (ConstString*)((const byte*)(*configValue->variable.reference)+configValue->offset);
              String_set(value,*configVariable.string);
            }
          }
        }
        else if (configValue->variable.string != NULL)
        {
          String_set(value,*configValue->variable.string);
        }

//TODO: compare with default
        if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
        if (!String_isEmpty(value))
        {
          String_escape(value,
                        STRING_ESCAPE_CHARACTER,
                        NULL,  // chars
                        STRING_ESCAPE_CHARACTERS_MAP_FROM,
                        STRING_ESCAPE_CHARACTERS_MAP_TO,
                        STRING_ESCAPE_CHARACTER_MAP_LENGTH
                      );
          String_quote(value,STRING_QUOTE,NULL);

          // write value
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%s = %S",indent,' ',configValue->name,value);
        }
        else
        {
          // write template
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C#%s = %s",indent,' ',configValue->name,configValue->templateText);
        }

        // free resources
        String_delete(value);
      }
      break;
    case CONFIG_VALUE_TYPE_SPECIAL:
      {
        String            line;
        bool              writtenFlag;
        ConfigValueFormat configValueFormat;

        // init variables
        line = String_new();

        // format init
        ConfigValue_formatInit(&configValueFormat,
                               configValue,
                               CONFIG_VALUE_FORMAT_MODE_LINE,
                               variable
                              );

        // format string
        writtenFlag = FALSE;
        while (ConfigValue_format(&configValueFormat,line))
        {
          // write
          if (error == ERROR_NONE) error = writeCommentLines(fileHandle,indent,commentList,configValue->commentList);
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C%S",indent,' ',line);

          writtenFlag = TRUE;
        }

        // format done
        ConfigValue_formatDone(&configValueFormat);

        // write template if no values written
        if (!writtenFlag && (configValue->specialValue.format != NULL))
        {
          String_format(line,"%*C#%s = ",indent,' ',configValue->name);
          configValue->specialValue.format(&configValueFormat.formatUserData,
                                           CONFIG_VALUE_OPERATION_TEMPLATE,
                                           line,
                                           configValue->specialValue.userData
                                          );
          if (error == ERROR_NONE) error = File_writeLine(fileHandle,line);
        }

        // free resources
        String_delete(line);
      }
      break;
    case CONFIG_VALUE_TYPE_IGNORE:
      break;
    case CONFIG_VALUE_TYPE_DEPRECATED:
#if 0
      // store value
      if (configValue->offset >= 0)
      {
        if (variable != NULL)
        {
          if (configValue->deprecatedValue.parse != NULL)
          {
            if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                    (byte*)variable+configValue->offset,
                                                    configValue->name,
                                                    value,
                                                    errorMessage,
                                                    sizeof(errorMessage)
                                                   )
               )
            {
              return FALSE;
            }
          }
        }
        else
        {
          if (configValue->variable.reference != NULL)
          {
            if (configValue->deprecatedValue.parse != NULL)
            {
              if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                      (byte*)(configValue->variable.reference)+configValue->offset,
                                                      configValue->name,
                                                      value,
                                                      errorMessage,
                                                      sizeof(errorMessage)
                                                     )
                 )
              {
                return FALSE;
              }
            }
          }
        }
      }
      else
      {
        if (variable != NULL)
        {
          if (configValue->deprecatedValue.parse != NULL)
          {
            if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                    variable,
                                                    configValue->name,
                                                    value,
                                                    errorMessage,
                                                    sizeof(errorMessage)
                                                   )
               )
            {
              return FALSE;
            }
          }
        }
        else
        {
          assert(configValue->variable.deprecated != NULL);

          if (configValue->deprecatedValue.parse != NULL)
          {
            if (!configValue->deprecatedValue.parse(configValue->deprecatedValue.userData,
                                                    configValue->variable.deprecated,
                                                    configValue->name,
                                                    value,
                                                    errorMessage,
                                                    sizeof(errorMessage)
                                                   )
               )
            {
              return FALSE;
            }
          }
        }
      }
#endif
      break;
    case CONFIG_VALUE_TYPE_SEPARATOR:
      if (configValue->separator.text != NULL)
      {
        if (error == ERROR_NONE)
        {
          error = File_printLine(fileHandle,
                                 "%*C# %-.3s %s %.*s",
                                 indent,' ',
                                 SEPARATOR,
                                 configValue->separator.text,
                                 (stringLength(SEPARATOR) > (3+1+stringLength(configValue->separator.text)+1))
                                   ? stringLength(SEPARATOR)-(3+1+stringLength(configValue->separator.text)+1)
                                   : 0,
                                 SEPARATOR
                                );
        }
      }
      else
      {
        if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C# %s",indent,' ',SEPARATOR);
      }
      break;
    case CONFIG_VALUE_TYPE_SPACE:
      if (error == ERROR_NONE) error = File_printLine(fileHandle,"");
      break;
    case CONFIG_VALUE_TYPE_COMMENT:
      if (!stringIsEmpty(configValue->comment.text))
      {
        if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C# %s",indent,' ',configValue->comment.text);
      }
      break;
    #ifndef NDEBUG
      default:
fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,configValue->type);
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  return error;
}

/***********************************************************************\
* Name   : writeConfigFile
* Purpose: write config file valules
* Input  : fileHandle                     - file handle
*          indent                         - indention
*          configValues                   - config values
*          firstValueIndex,lastValueIndex - first/last value index
*          variable                       - variable
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeConfigFile(FileHandle        *fileHandle,
                             uint              indent,
                             const ConfigValue configValues[],
                             uint              firstValueIndex,
                             uint              lastValueIndex,
                             const void        *variable
                            )
{
  Errors     error;
  uint       index;
  StringList commentList;

  // init variables

  // write values
  error = ERROR_NONE;
  StringList_init(&commentList);
  ITERATE_VALUEX(configValues,index,firstValueIndex,lastValueIndex,error == ERROR_NONE)
  {
//fprintf(stderr,"%s, %d: %d: %s\n",__FILE__,__LINE__,configValues[index].type,configValues[index].name);
    switch (configValues[index].type)
    {
      case CONFIG_VALUE_TYPE_BEGIN_SECTION:
        {
          uint   sectionFirstValueIndex,sectionLastValueIndex;
          void   *sectionIterator;
          String name;
          void   *data;

          if (   (configValues[index+1].type != CONFIG_VALUE_TYPE_END_SECTION)
              && (configValues[index+1].type != CONFIG_VALUE_TYPE_END)
             )
          {
            // find section
            sectionFirstValueIndex = index+1;
            sectionLastValueIndex  = sectionFirstValueIndex;
            while (   (configValues[sectionLastValueIndex+1].type != CONFIG_VALUE_TYPE_END_SECTION)
                   && (configValues[sectionLastValueIndex+1].type != CONFIG_VALUE_TYPE_END)
                  )
            {
              sectionLastValueIndex++;
            }

            error = flushCommentLines(fileHandle,indent,&commentList);
fprintf(stderr,"%s, %d: %s %d\n",__FILE__,__LINE__,configValues[index].name,List_count(&commentList));
            error = writeCommentLines(fileHandle,indent,NULL,configValues[index].commentList);

            // init iterator
            if (configValues[index].section.sectionIterator != NULL)
            {
              configValues[index].section.sectionIterator(&sectionIterator,
                                                          CONFIG_VALUE_OPERATION_INIT,
                                                          configValues[index].variable.pointer,
                                                          configValues[index].section.userData
                                                         );
            }

            if (configValues[index].section.sectionIterator != NULL)
            {
              // iterate
              name = String_new();
              do
              {
#if 1
                const StringList *commentList = configValues[index].section.sectionIterator(&sectionIterator,
                                                                                            CONFIG_VALUE_OPERATION_COMMENTS,
                                                                                            name,
                                                                                            configValues[index].section.userData
                                                                                           );
                if (commentList != NULL)
                {
                  StringNode *stringNode;
                  String     line;

                  // write lines
                  STRINGLIST_ITERATE(commentList,stringNode,line)
                  {
                    error = File_printLine(fileHandle,"# %S",line);
                    if (error != ERROR_NONE) break;
                  }
                }
#endif

                data = configValues[index].section.sectionIterator(&sectionIterator,
                                                                   CONFIG_VALUE_OPERATION,
                                                                   name,
                                                                   configValues[index].section.userData
                                                                  );
                if (data != NULL)
                {
                  // write section begin
                  if (!String_isEmpty(name))
                  {
                    if (error == ERROR_NONE) error = File_printLine(fileHandle,"[%s '%S']",configValues[index].name,name);
                  }
                  else
                  {
                    if (error == ERROR_NONE) error = File_printLine(fileHandle,"[%s]",configValues[index].name);
                  }

                  // write section
                  if (error == ERROR_NONE) error = writeConfigFile(fileHandle,
                                                                   indent+2,
                                                                   configValues,
                                                                   sectionFirstValueIndex,
                                                                   sectionLastValueIndex,
                                                                   data
                                                                  );

                  // write section end
                  if (error == ERROR_NONE) error = File_printLine(fileHandle,"[end]");
                  if (error == ERROR_NONE) error = File_printLine(fileHandle,"");
                }
              }
              while ((data != NULL) && (error == ERROR_NONE));
              String_delete(name);
            }
            else
            {
              // write section begin
              if (error == ERROR_NONE) error = File_printLine(fileHandle,"[%s]",configValues[index].name);

              // write section
              if (error == ERROR_NONE) error = writeConfigFile(fileHandle,
                                                               indent+2,
                                                               configValues,
                                                               sectionFirstValueIndex,
                                                               sectionLastValueIndex,
                                                               NULL  // variable
                                                              );

              // write section end
              if (error == ERROR_NONE) error = File_printLine(fileHandle,"[end]");
              if (error == ERROR_NONE) error = File_printLine(fileHandle,"");
            }

            // done iterator
            if (configValues[index].section.sectionIterator != NULL)
            {
              configValues[index].section.sectionIterator(&sectionIterator,
                                                          CONFIG_VALUE_OPERATION_DONE,
                                                          NULL, // data
                                                          configValues[index].section.userData
                                                         );
            }
          }

          // done section
          index = sectionLastValueIndex+1;
        }
        break;
      case CONFIG_VALUE_TYPE_END_SECTION:
        // nothing to do
        break;
      case CONFIG_VALUE_TYPE_SEPARATOR:
        error = flushCommentLines(fileHandle,indent,&commentList);

        if (configValues[index].separator.text != NULL)
        {
          if (error == ERROR_NONE)
          {
            error = File_printLine(fileHandle,
                                   "# %-.3s %s %.*s",
                                   SEPARATOR,
                                   configValues[index].separator.text,
                                   (stringLength(SEPARATOR) > (3+1+stringLength(configValues[index].separator.text)+1))
                                     ? stringLength(SEPARATOR)-(3+1+stringLength(configValues[index].separator.text)+1)
                                     : 0,
                                   SEPARATOR
                                  );
          }
        }
        else
        {
          if (error == ERROR_NONE) error = File_printLine(fileHandle,"%*C# %s",indent,' ',SEPARATOR);
        }
        break;
      case CONFIG_VALUE_TYPE_SPACE:
        error = flushCommentLines(fileHandle,indent,&commentList);

        if (error == ERROR_NONE) error = File_printLine(fileHandle,"");
        break;
      case CONFIG_VALUE_TYPE_COMMENT:
        if (!stringIsEmpty(configValues[index].comment.text))
        {
          StringList_appendFormat(&commentList,"%s",configValues[index].comment.text);
        }
        break;
      default:
//fprintf(stderr,"%s, %d: %s %d\n",__FILE__,__LINE__,configValues[index].name,List_count(&commentList));
//File_printLine(fileHandle,"a %d %d",List_count(&commentList),(configValues[index].commentList!=0) ? List_count(configValues[index].commentList) : -1);
        error = writeValue(fileHandle,
                           indent,
                           &configValues[index],
                           variable,
                           &commentList
                          );
        StringList_clear(&commentList);
//File_printLine(fileHandle,"b");
        break;
    }
  }
  StringList_done(&commentList);

  // free resources

  return ERROR_NONE;
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
  uint index;

  assert(configValues != NULL);

  index = 0;
  while (configValues[index].type != CONFIG_VALUE_TYPE_END)
  {
    if (configValues[index].commentList != NULL)
    {
      StringList_delete(configValues[index].commentList);
      configValues[index].commentList = NULL;
    }
    index++;
  }
}

bool ConfigValue_findSection(const ConfigValue configValues[],
                             const char        *sectionName,
                             uint              *sectionIndex,
                             uint              *firstValueIndex,
                             uint              *lastValueIndex
                            )
{
  uint index;
  bool skipFlag;

  assert(configValues != NULL);
  assert(sectionName != NULL);

  index = 0;
  if (sectionName != NULL)
  {
    // find section
    while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
           && (   (configValues[index].type != CONFIG_VALUE_TYPE_BEGIN_SECTION)
               || !stringEquals(configValues[index].name,sectionName)
              )
          )
    {
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
    if (configValues[index].type != CONFIG_VALUE_TYPE_BEGIN_SECTION)
    {
      return FALSE;
    }
    if (sectionIndex != NULL) (*sectionIndex) = index;
    index++;

    // get first index
    if (firstValueIndex != NULL) (*firstValueIndex) = index;

    // find section end
    while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
           && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
          )
    {
      index++;
    }
    if (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
    {
      return FALSE;
    }
    index--;

    // get last index
    if (lastValueIndex != NULL) (*lastValueIndex) = index;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

uint ConfigValue_valueIndex(const ConfigValue configValues[],
                            const char        *sectionName,
                            const char        *name
                           )
{
  uint index;

  assert(configValues != NULL);
  assert(name != NULL);

  index = ConfigValue_firstValueIndex(configValues,sectionName);
  while (   (index != CONFIG_VALUE_INDEX_NONE)
         && (configValues[index].type != CONFIG_VALUE_TYPE_END)
         && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
         && !stringEquals(configValues[index].name,name)
        )
  {
    index = ConfigValue_nextValueIndex(configValues,index);
  }

  return (   (index != CONFIG_VALUE_INDEX_NONE)
          && (configValues[index].type != CONFIG_VALUE_TYPE_END)
          && (configValues[index].type != CONFIG_VALUE_TYPE_END_SECTION)
         )
           ? index
           : CONFIG_VALUE_INDEX_NONE;
}

uint ConfigValue_find(const ConfigValue configValues[],
                      uint              firstValueIndex,
                      uint              lastValueIndex,
                      const char        *name
                     )
{
  uint index;

  assert(configValues != NULL);
  assert(name != NULL);

  index = (firstValueIndex != CONFIG_VALUE_INDEX_NONE) ? firstValueIndex : 0;
  if (lastValueIndex != CONFIG_VALUE_INDEX_NONE)
  {
    while (   (index <= lastValueIndex)
           && !stringEquals(configValues[index].name,name)
          )
    {
      index = ConfigValue_nextValueIndex(configValues,index);
    }
  }
  else
  {
    while (   (configValues[index].type != CONFIG_VALUE_TYPE_END)
           && !stringEquals(configValues[index].name,name)
          )
    {
      index = ConfigValue_nextValueIndex(configValues,index);
    }
  }

  return index;
}

uint ConfigValue_firstValueIndex(const ConfigValue configValues[],
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
      // skip section
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
    if (configValues[index].type == CONFIG_VALUE_TYPE_BEGIN_SECTION) index++;
  }
  else
  {
    // skip sections
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
        case CONFIG_VALUE_TYPE_END:
        default:
          skipFlag = FALSE;
          break;
      }
    }
    while (skipFlag);
  }

  return (configValues[index].type != CONFIG_VALUE_TYPE_END) ? index : CONFIG_VALUE_INDEX_NONE;
}

uint ConfigValue_lastValueIndex(const ConfigValue configValues[],
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
      // skip section
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
      // skip sections
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

      // skip sections
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
          case CONFIG_VALUE_TYPE_END:
          default:
            skipFlag = FALSE;
            break;
        }
      }
      while (skipFlag);
    }
  }

  return (configValues[index].type != CONFIG_VALUE_TYPE_END) ? index : CONFIG_VALUE_INDEX_NONE;
}

uint ConfigValue_nextValueIndex(const ConfigValue configValues[],
                                uint              index
                               )
{
  bool skipFlag;

  assert(configValues != NULL);
  assert(index != CONFIG_VALUE_INDEX_NONE);

  if (configValues[index].type != CONFIG_VALUE_TYPE_END)
  {
    index++;

    // skip sections
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
        case CONFIG_VALUE_TYPE_END:
        default:
          skipFlag = FALSE;
          break;
      }
    }
    while (skipFlag);
  }

  return (configValues[index].type != CONFIG_VALUE_TYPE_END) ? index : CONFIG_VALUE_INDEX_NONE;
}

bool ConfigValue_parse(const char           *name,
                       const char           *value,
                       ConfigValue          configValues[],
                       const char           *sectionName,
                       ConfigReportFunction errorReportFunction,
                       void                 *errorReportUserData,
                       ConfigReportFunction warningReportFunction,
                       void                 *warningReportUserData,
                       void                 *variable,
                       StringList           *commentLineList
                      )
{
  uint i;

  assert(name != NULL);
  assert(configValues != NULL);

  // find config value
  i = ConfigValue_valueIndex(configValues,sectionName,name);
  if (i == CONFIG_VALUE_INDEX_NONE)
  {
    reportMessage(errorReportFunction,
                  errorReportUserData,
                  "Unknown value '%s'!",
                  name
                 );
    return FALSE;
  }

  // process value
  if (!processValue(&configValues[i],
                    sectionName,
                    value,
                    errorReportFunction,
                    errorReportUserData,
                    warningReportFunction,
                    warningReportUserData,
                    variable
                   ))
  {
    return FALSE;
  }

  if ((commentLineList != NULL) && !StringList_isEmpty(commentLineList))
  {
    if (configValues[i].commentList == NULL)
    {
      configValues[i].commentList = StringList_new();
    }
fprintf(stderr,"%s, %d: %s %d\n",__FILE__,__LINE__,name,List_count(commentLineList));
    StringList_clear(configValues[i].commentList);
    StringList_move(configValues[i].commentList,commentLineList);
//StringList_clear(commentLineList);
fprintf(stderr,"%s, %d: %s %d %d\n",__FILE__,__LINE__,name,List_count(configValues[i].commentList),List_count(commentLineList));
  }

  return TRUE;
}
bool ConfigValue_parse2(ConfigValue          *configValue,
                       const char           *sectionName,
                       const char           *value,
                       ConfigReportFunction errorReportFunction,
                       void                 *errorReportUserData,
                       ConfigReportFunction warningReportFunction,
                       void                 *warningReportUserData,
                       void                 *variable,
                       StringList           *commentLineList
                      )
{
//  assert(name != NULL);
  assert(configValue != NULL);

  if (!processValue(configValue,
                    sectionName,
                    value,
                    errorReportFunction,
                    errorReportUserData,
                    warningReportFunction,
                    warningReportUserData,
                    variable
                   ))
  {
    return FALSE;
  }

  if ((commentLineList != NULL) && !StringList_isEmpty(commentLineList))
  {
    if (configValue->commentList == NULL)
    {
      configValue->commentList = StringList_new();
    }
fprintf(stderr,"%s, %d: %s %d\n",__FILE__,__LINE__,configValue->name,List_count(commentLineList));
    StringList_clear(configValue->commentList);
    StringList_move(configValue->commentList,commentLineList);
//StringList_clear(commentLineList);
fprintf(stderr,"%s, %d: %s %d %d\n",__FILE__,__LINE__,configValue->name,List_count(configValue->commentList),List_count(commentLineList));
  }

  return TRUE;
}

bool ConfigValue_parseDeprecatedBoolean(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  if (value != NULL)
  {
    if      (   stringEqualsIgnoreCase(value,"1")
             || stringEqualsIgnoreCase(value,"true")
             || stringEqualsIgnoreCase(value,"on")
             || stringEqualsIgnoreCase(value,"yes")
            )
    {
      (*(bool*)variable) = TRUE;
    }
    else if (   stringEqualsIgnoreCase(value,"0")
             || stringEqualsIgnoreCase(value,"false")
             || stringEqualsIgnoreCase(value,"off")
             || stringEqualsIgnoreCase(value,"no")
            )
    {
      (*(bool*)variable) = FALSE;
    }
    else
    {
      stringFormat(errorMessage,errorMessageSize,"expected boolean value: yes|no");
      return FALSE;
    }
  }
  else
  {
    (*(bool*)variable) = FALSE;
  }

  return TRUE;
}

bool ConfigValue_parseDeprecatedInteger(void       *userData,
                                        void       *variable,
                                        const char *name,
                                        const char *value,
                                        char       errorMessage[],
                                        uint       errorMessageSize
                                       )
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(int*)variable) = strtol(value,NULL,0);

  return TRUE;
}

bool ConfigValue_parseDeprecatedInteger64(void       *userData,
                                          void       *variable,
                                          const char *name,
                                          const char *value,
                                          char       errorMessage[],
                                          uint       errorMessageSize
                                         )
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(int64*)variable) = strtoll(value,NULL,0);

  return TRUE;
}

bool ConfigValue_parseDeprecatedString(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String string;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (value != NULL)
  {
    // unquote/unescape
    string = String_newCString(value);
    String_unquote(string,STRING_QUOTES);
    String_unescape(string,
                    STRING_ESCAPE_CHARACTER,
                    STRING_ESCAPE_CHARACTERS_MAP_TO,
                    STRING_ESCAPE_CHARACTERS_MAP_FROM,
                    STRING_ESCAPE_CHARACTER_MAP_LENGTH
                  );

    String_set(*((String*)variable),string);

    // free resources
    String_delete(string);
  }
  else
  {
    String_clear(*((String*)variable));
  }

  return TRUE;
}

void ConfigValue_addComments(ConfigValue *configValue,
                             StringList  *commentList
                            )
{
  assert(configValue != NULL);

  if (configValue->commentList == NULL)
  {
    configValue->commentList = StringList_new();
  }
  StringList_move(configValue->commentList,commentList);
}

bool ConfigValue_getIntegerValue(int                   *value,
                                 const char            *string,
                                 const ConfigValueUnit *units
                                )
{
  return getIntegerValue(value,string,units,NULL,NULL);
}

bool ConfigValue_getInteger64Value(int64                 *value,
                                   const char            *string,
                                   const ConfigValueUnit *units
                                  )
{
  return getInteger64Value(value,string,units,NULL,NULL);
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
      if (configValue->specialValue.format != NULL)
      {
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configValue->specialValue.format(&configValueFormat->formatUserData,
                                             CONFIG_VALUE_OPERATION_INIT,
                                             (byte*)variable+configValueFormat->configValue->offset,
                                             configValue->specialValue.userData
                                            );
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            if ((*configValueFormat->configValue->variable.reference) != NULL)
            {
              configValue->specialValue.format(&configValueFormat->formatUserData,
                                               CONFIG_VALUE_OPERATION_INIT,
                                               (byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset,
                                               configValue->specialValue.userData
                                              );
            }
          }
        }
        else if (configValueFormat->configValue->variable.special != NULL)
        {
          configValue->specialValue.format(&configValueFormat->formatUserData,
                                           CONFIG_VALUE_OPERATION_INIT,
                                           configValueFormat->configValue->variable.special,
                                           configValue->specialValue.userData
                                          );
        }
      }
      else
      {
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (variable != NULL)
          {
            configValueFormat->formatUserData = (byte*)variable+configValueFormat->configValue->offset;
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            if ((*configValueFormat->configValue->variable.reference) != NULL)
            {
              configValueFormat->formatUserData = (byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset;
            }
          }
        }
        else if (configValueFormat->configValue->variable.special != NULL)
        {
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
    case CONFIG_VALUE_TYPE_SEPARATOR:
    case CONFIG_VALUE_TYPE_SPACE:
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
      if (configValueFormat->configValue->specialValue.format != NULL)
      {
        configValueFormat->configValue->specialValue.format(&configValueFormat->formatUserData,
                                                            CONFIG_VALUE_OPERATION_DONE,
                                                            configValueFormat->configValue->variable.special,
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
    case CONFIG_VALUE_TYPE_SEPARATOR:
    case CONFIG_VALUE_TYPE_SPACE:
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
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.i = (int*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.i = (int*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
          else
          {
            return FALSE;
          }
        }
        else if (configValueFormat->configValue->variable.i != NULL)
        {
          configVariable.i = configValueFormat->configValue->variable.i;
        }
        else
        {
          return FALSE;
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
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.l = (int64*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.l = (int64*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
          else
          {
            return FALSE;
          }
        }
        else if (configValueFormat->configValue->variable.l != NULL)
        {
          configVariable.l = configValueFormat->configValue->variable.l;
        }
        else
        {
          return FALSE;
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
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.d = (double*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.d = (double*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        else
        {
          return FALSE;
        }
        }
        else if (configValueFormat->configValue->variable.d != NULL)
        {
          configVariable.d = configValueFormat->configValue->variable.d;
        }
        else
        {
          return FALSE;
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
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.b = (bool*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.b = (bool*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
          else
          {
            return FALSE;
          }
        }
        else if (configValueFormat->configValue->variable.b != NULL)
        {
          configVariable.b = configValueFormat->configValue->variable.b;
        }
        else
        {
          return FALSE;
        }

        // format value
        String_appendFormat(line,"%s",(*configVariable.b) ? "yes":"no");

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_ENUM:
        // get value
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.enumeration = (uint*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.enumeration = (uint*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
          else
          {
            return FALSE;
          }
        }
        else if (configValueFormat->configValue->variable.enumeration != NULL)
        {
          configVariable.enumeration = configValueFormat->configValue->variable.enumeration;
        }
        else
        {
          return FALSE;
        }

        // format value
        String_appendFormat(line,"%d",configVariable.enumeration);

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_SELECT:
        // get value
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.select = (uint*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.select = (uint*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        else
        {
          return FALSE;
        }
        }
        else if (configValueFormat->configValue->variable.select != NULL)
        {
          configVariable.select = configValueFormat->configValue->variable.select;
        }
        else
        {
          return FALSE;
        }
        select = findSelectByValue(configValueFormat->configValue->selectValue.selects,*configVariable.select);

        // format value
        String_appendFormat(line,"%s",(select != NULL) ? select->name : "");

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_SET:
        // get value
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.set = (ulong*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.set = (ulong*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
          else
          {
            return FALSE;
          }
        }
        else if (configValueFormat->configValue->variable.set != NULL)
        {
          configVariable.set = configValueFormat->configValue->variable.set;
        }
        else
        {
          return FALSE;
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
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.cString = (char**)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.cString = (char**)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
          else
          {
            return FALSE;
          }
        }
        else if (configValueFormat->configValue->variable.cString != NULL)
        {
          configVariable.cString = configValueFormat->configValue->variable.cString;
        }
        else
        {
          return FALSE;
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
        if      (configValueFormat->configValue->offset >= 0)
        {
          if      (configValueFormat->variable != NULL)
          {
            configVariable.string = (String*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else if (configValueFormat->configValue->variable.reference != NULL)
          {
            configVariable.string = (String*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
          else
          {
            return FALSE;
          }
        }
        else if (configValueFormat->configValue->variable.string != NULL)
        {
          configVariable.string = configValueFormat->configValue->variable.string;
        }
        else
        {
          return FALSE;
        }

        // format value
        String_appendFormat(line,"%'S",*configVariable.string);

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_SPECIAL:
        if (configValueFormat->configValue->specialValue.format != NULL)
        {
          configValueFormat->endOfDataFlag = !configValueFormat->configValue->specialValue.format(&configValueFormat->formatUserData,
                                                                                                  CONFIG_VALUE_OPERATION,
                                                                                                  line,
                                                                                                  configValueFormat->configValue->specialValue.userData
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
      case CONFIG_VALUE_TYPE_SEPARATOR:
      case CONFIG_VALUE_TYPE_SPACE:
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
  Errors     error;
  FileHandle fileHandle;
  StringNode *stringNode;
  String     line;

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

Errors ConfigValue_writeConfigFile(ConstString       configFileName,
                                   const ConfigValue configValues[]
                                  )
{
  Errors     error;
  FileHandle fileHandle;
  uint       index;

  assert(configFileName != NULL);
  assert(configValues != NULL);
  assert(configValues[0].type != CONFIG_VALUE_TYPE_END);

  // init variables

  // open file
  error = File_open(&fileHandle,configFileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get last config value
  index = 0;
  while (configValues[index+1].type != CONFIG_VALUE_TYPE_END)
  {
    index++;
  }

  // write file
  error = writeConfigFile(&fileHandle,
                          0,
                          configValues,
                          0,
                          index,
                          NULL  // variable
                         );
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    return error;
  }

  // close file
  File_close(&fileHandle);

  // set permissions
  error = File_setPermission(configFileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}

void ConfigValue_listSectionDataIteratorInit(ConfigValueSectionDataIterator *sectionDataIterator, void *variable, void *userData)
{
  assert(sectionDataIterator != NULL);
  assert(variable != NULL);

  UNUSED_VARIABLE(userData);

  (*sectionDataIterator) = List_first((List*)variable);
}

void ConfigValue_listSectionDataIteratorDone(ConfigValueSectionDataIterator *sectionDataIterator, void *userData)
{
  assert(sectionDataIterator != NULL);

  UNUSED_VARIABLE(userData);
}

void *ConfigValue_listSectionDataIteratorNext(ConfigValueSectionDataIterator *sectionDataIterator, void *userData)
{
  Node *node = (Node*)(*sectionDataIterator);

  assert(sectionDataIterator != NULL);

  UNUSED_VARIABLE(userData);

  if (node != NULL)
  {
    (*sectionDataIterator) = node->next;
  }

  return node;
}

void *ConfigValue_listSectionDataIterator(ConfigValueSectionDataIterator *sectionDataIterator, ConfigValueOperations operation, void *data, void *userData)
{
  void *result;

  assert(sectionDataIterator != NULL);

  UNUSED_VARIABLE(userData);

  result = NULL;

  switch (operation)
  {
    case CONFIG_VALUE_OPERATION_INIT:
      assert(data != NULL);

      (*sectionDataIterator) = List_first((List*)data);
      break;
    case CONFIG_VALUE_OPERATION_DONE:
      break;
    case CONFIG_VALUE_OPERATION_TEMPLATE:
      break;
    case CONFIG_VALUE_OPERATION:
      {
        Node   *node = (Node*)(*sectionDataIterator);
        String name  = (String)data;

        assert(name != NULL);

        if (node != NULL)
        {
          (*sectionDataIterator) = node->next;
        }
        String_clear(name);

        result = node;
      }
   }

   return result;
}

#ifdef __GNUC__
  #pragma GCC pop_options
#endif /* __GNUC__ */

#ifdef __GNUG__
}
#endif

/* end of file */
