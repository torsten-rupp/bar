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
#include "strings.h"

#include "configvalues.h"

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
* Name   : getIntegerOption
* Purpose: get integer value
* Input  : value             - value variable
*          string            - string
*          units             - units array or NULL
*          unitCount         - size of unit array
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getIntegerValue(int                   *value,
                           const char            *string,
                           const ConfigValueUnit *units,
                           uint                  unitCount,
                           FILE                  *errorOutputHandle,
                           const char            *errorPrefix
                          )
{
  uint  i,j,z;
  char  number[128],unit[32];
  ulong factor;

  assert(value != NULL);
  assert(string != NULL);

  // split number, unit
  i = strlen(string);
  if (i > 0)
  {
    while ((i > 1) && !isdigit(string[i-1])) { i--; }
    j = MIN(i,               sizeof(number)-1); strncpy(number,&string[0],j); number[j] = '\0';
    j = MIN(strlen(string)-i,sizeof(unit)  -1); strncpy(unit,  &string[i],j); unit[j]   = '\0';
  }
  else
  {
    number[0] = '\0';
    unit[0]   = '\0';
  }

  // find factor
  if (unit[0] != '\0')
  {
    if (units != NULL)
    {
      z = 0;
      while (   (z < unitCount)
             && !stringEquals(units[z].name,unit)
            )
      {
        z++;
      }
      if (z >= unitCount)
      {
        if (errorOutputHandle != NULL)
        {
          fprintf(errorOutputHandle,
                  "%sInvalid unit in integer value '%s'! Valid units:",
                  (errorPrefix != NULL) ? errorPrefix : "",
                  string
                 );
          for (z = 0; z < unitCount; z++)
          {
            fprintf(errorOutputHandle," %s",units[z].name);
          }
          fprintf(errorOutputHandle,".\n");
        }
        return FALSE;
      }
      factor = units[z].factor;
    }
    else
    {
      if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
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
* Input  : value             - value variable
*          string            - string
*          units             - units array or NULL
*          unitCount         - size of unit array
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getInteger64Value(int64                 *value,
                             const char            *string,
                             const ConfigValueUnit *units,
                             uint                  unitCount,
                             FILE                  *errorOutputHandle,
                             const char            *errorPrefix
                            )
{
  uint  i,j,z;
  char  number[128],unit[32];
  ulong factor;

  assert(value != NULL);
  assert(string != NULL);

  // split number, unit
  i = strlen(string);
  if (i > 0)
  {
    while ((i > 1) && !isdigit(string[i-1])) { i--; }
    j = MIN(i,               sizeof(number)-1); strncpy(number,&string[0],j); number[j] = '\0';
    j = MIN(strlen(string)-i,sizeof(unit)  -1); strncpy(unit,  &string[i],j); unit[j]   = '\0';
  }
  else
  {
    number[0] = '\0';
    unit[0]   = '\0';
  }

  // find factor
  if (unit[0] != '\0')
  {
    if (units != NULL)
    {
      z = 0;
      while (   (z < unitCount)
             && !stringEquals(units[z].name,unit)
            )
      {
        z++;
      }
      if (z >= unitCount)
      {
        if (errorOutputHandle != NULL)
        {
          fprintf(errorOutputHandle,
                  "%sInvalid unit in integer value '%s'! Valid units:",
                  (errorPrefix != NULL) ? errorPrefix : "",
                  string
                 );
          for (z = 0; z < unitCount; z++)
          {
            fprintf(errorOutputHandle," %s",units[z].name);
          }
          fprintf(errorOutputHandle,".\n");
        }
        return FALSE;
      }
      factor = units[z].factor;
    }
    else
    {
      if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
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
* Input  : configValue       - config value
*          prefix            - option prefix ("-" or "--")
*          name              - option name
*          value             - option value or NULL
*          errorOutputHandle - error output handle or NULL
*          errorPrefix       - error prefix or NULL
* Output : variable - variable to store value
* Return : TRUE if option processed without error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool processValue(const ConfigValue *configValue,
                        const char        *name,
                        const char        *value,
                        FILE              *errorOutputHandle,
                        const char        *errorPrefix,
                        void              *variable
                       )
{
  ConfigVariable configVariable;
  char           errorMessage[256];

  assert(configValue != NULL);
  assert(name != NULL);

  switch (configValue->type)
  {
    case CONFIG_VALUE_TYPE_INTEGER:
      {
        int data;

        // get integer option value
        if (!getIntegerValue(&data,
                             value,
                             configValue->integerValue.units,
                             configValue->integerValue.unitCount,
                             errorOutputHandle,
                             errorPrefix
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
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sValue '%s' out range %d..%d for config value '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
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
                               configValue->integer64Value.unitCount,
                               errorOutputHandle,
                               errorPrefix
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
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sValue '%s' out range %lld..%lld for config value '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
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
        uint   i,j,z;
        char   number[128],unit[32];
        ulong  factor;
        double data;

        // split number, unit
        i = strlen(value);
        if (i > 0)
        {
          while ((i > 1) && !isdigit(value[i-1])) { i--; }
          j = MIN(i,              sizeof(number)-1); strncpy(number,&value[0],j);number[j] = '\0';
          j = MIN(strlen(value)-i,sizeof(unit)  -1); strncpy(unit,  &value[i],j);unit[j]   = '\0';
        }
        else
        {
          number[0] = '\0';
          unit[0]   = '\0';
        }

        // find factor
        if (unit[0] != '\0')
        {
          if (configValue->doubleValue.units != NULL)
          {
            z = 0;
            while (   (z < configValue->doubleValue.unitCount)
                   && stringEquals(configValue->doubleValue.units[z].name,unit)
                  )
            {
              z++;
            }
            if (z >= configValue->doubleValue.unitCount)
            {
              if (errorOutputHandle != NULL)
              {
                fprintf(errorOutputHandle,
                        "%sInvalid unit in float value '%s'! Valid units:",
                        (errorPrefix != NULL)?errorPrefix:"",
                        value
                       );
                for (z = 0; z < configValue->integerValue.unitCount; z++)
                {
                  fprintf(errorOutputHandle," %s",configValue->integerValue.units[z].name);
                }
                fprintf(errorOutputHandle,".\n");
              }
              return FALSE;
            }
            factor = configValue->doubleValue.units[z].factor;
          }
          else
          {
            if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                   "%sUnexpected unit in value '%s'!\n",
                                                   (errorPrefix != NULL)?errorPrefix:"",
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
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sValue '%s' out range %lf..%lf for float config value '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
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
                 || stringEqualsIgnoreCase(value,"1")
                 || stringEqualsIgnoreCase(value,"true")
                 || stringEqualsIgnoreCase(value,"on")
                 || stringEqualsIgnoreCase(value,"yes")
                )
        {
          data = TRUE;
        }
        else if (   stringEqualsIgnoreCase(value,"0")
                 || stringEqualsIgnoreCase(value,"false")
                 || stringEqualsIgnoreCase(value,"off")
                 || stringEqualsIgnoreCase(value,"no")
                )
        {
          data = FALSE;
        }
        else
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sInvalid value '%s' for boolean config value '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
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
        uint z;

        // find select value
        z = 0;
        while (   (z < configValue->selectValue.selectCount)
               && !stringEquals(configValue->selectValue.select[z].name,value)
              )
        {
          z++;
        }
        if (z >= configValue->selectValue.selectCount)
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sUnknown value '%s' for config value '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
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
            (*configVariable.select) = configValue->selectValue.select[z].value;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.select = (uint*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.select) = configValue->selectValue.select[z].value;
            }
          }
        }
        else
        {
          assert(configValue->variable.select != NULL);
          (*configValue->variable.select) = configValue->selectValue.select[z].value;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_SET:
      {
        uint i,j,z;
        char setName[128];

        // find and store set values
        assert(configValue->variable.set != NULL);
        i = 0;
        while (value[i] != '\0')
        {
          // skip spaces
          while ((value[i] != '\0') && isspace(value[i])) { i++; }

          // get name
          j = 0;
          while ((value[i] != '\0') && (value[i] != ','))
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
            z = 0;
            while ((z < configValue->setValue.setCount) && !stringEquals(configValue->setValue.set[z].name,setName))
            {
              z++;
            }
            if (z >= configValue->setValue.setCount)
            {
              if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                     "%sUnknown value '%s' for config value '%s'!\n",
                                                     (errorPrefix != NULL)?errorPrefix:"",
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
                (*configVariable.set) |= configValue->setValue.set[z].value;
              }
              else
              {
                assert(configValue->variable.reference != NULL);
                if ((*configValue->variable.reference) != NULL)
                {
                  configVariable.set = (ulong*)((byte*)(*configValue->variable.reference)+configValue->offset);
                  (*configVariable.set) |= configValue->setValue.set[z].value;
                }
              }
            }
            else
            {
              assert(configValue->variable.set != NULL);
              (*configValue->variable.set) |= configValue->setValue.set[z].value;
            }
          }
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
            if (errorOutputHandle != NULL)
            {
              errorMessage[sizeof(errorMessage)-1] = '\0';
              if (strlen(errorMessage) > 0)
              {
                fprintf(errorOutputHandle,
                        "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                        (errorPrefix != NULL)?errorPrefix:"",
                        value,
                        configValue->name,
                        errorMessage
                       );
              }
              else
              {
                fprintf(errorOutputHandle,
                        "%sInvalid value '%s' for config value '%s'!\n",
                        (errorPrefix != NULL)?errorPrefix:"",
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
              if (errorOutputHandle != NULL)
              {
                errorMessage[sizeof(errorMessage)-1] = '\0';
                if (strlen(errorMessage) > 0)
                {
                  fprintf(errorOutputHandle,
                          "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                          (errorPrefix != NULL)?errorPrefix:"",
                          value,
                          configValue->name,
                          errorMessage
                         );
                }
                else
                {
                  fprintf(errorOutputHandle,
                          "%sInvalid value '%s' for config value '%s'!\n",
                          (errorPrefix != NULL)?errorPrefix:"",
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
          if (errorOutputHandle != NULL)
          {
            errorMessage[sizeof(errorMessage)-1] = '\0';
            if (strlen(errorMessage) > 0)
            {
              fprintf(errorOutputHandle,
                      "%sInvalid value '%s' for config value '%s' (error: %s)!\n",
                      (errorPrefix != NULL)?errorPrefix:"",
                      value,
                      configValue->name,
                      errorMessage
                     );
            }
            else
            {
              fprintf(errorOutputHandle,
                      "%sInvalid value '%s' for config value '%s'!\n",
                      (errorPrefix != NULL)?errorPrefix:"",
                      value,
                      configValue->name
                     );
            }
          }
          return FALSE;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_BEGIN_SECTION:
    case CONFIG_VALUE_TYPE_END_SECTION:
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

bool ConfigValue_init(const ConfigValue configValues[],
                      uint              configValueCount
                     )
{
  assert(configValues != NULL);

  UNUSED_VARIABLE(configValues);
  UNUSED_VARIABLE(configValueCount);

  return TRUE;
}

void ConfigValue_done(const ConfigValue configValues[],
                      uint              configValueCount
                     )
{
  assert(configValues != NULL);

  UNUSED_VARIABLE(configValues);
  UNUSED_VARIABLE(configValueCount);
}

bool ConfigValue_parse(const char        *name,
                       const char        *value,
                       const ConfigValue configValues[],
                       uint              configValueCount,
                       const char        *sectionName,
                       FILE              *errorOutputHandle,
                       const char        *errorPrefix,
                       void              *variable
                      )
{
  uint i,j;

  assert(name != NULL);
  assert(configValues != NULL);

  // find config value
  if (sectionName != NULL)
  {
    i = ConfigValue_firstSectionValue(configValues,configValueCount,sectionName);
    j = ConfigValue_endSectionValue(configValues,configValueCount,i);
    while (   (i < j)
           && !stringEquals(configValues[i].name,name)
          )
    {
      i = ConfigValue_nextSectionValue(configValues,configValueCount,i);
    }
  }
  else
  {
    i = ConfigValue_firstValue(configValues,configValueCount);
    j = ConfigValue_endValue(configValues,configValueCount,i);
    while (   (i < j)
           && !stringEquals(configValues[i].name,name)
          )
    {
      i = ConfigValue_nextValue(configValues,configValueCount,i);
    }
  }
  if (i >= j)
  {
    return FALSE;
  }

  // process value
  if (!processValue(&configValues[i],name,value,errorOutputHandle,errorPrefix,variable))
  {
    return FALSE;
  }

  return TRUE;
}

bool ConfigValue_getIntegerValue(int                   *value,
                                 const char            *string,
                                 const ConfigValueUnit *units,
                                 uint                  unitCount
                                )
{
  return getIntegerValue(value,string,units,unitCount,NULL,NULL);
}

bool ConfigValue_getInteger64Value(int64                 *value,
                                   const char            *string,
                                   const ConfigValueUnit *units,
                                   uint                  unitCount
                                  )
{
  return getInteger64Value(value,string,units,unitCount,NULL,NULL);
}

void ConfigValue_formatInit(ConfigValueFormat      *configValueFormat,
                            const ConfigValue      *configValue,
                            ConfigValueFormatModes mode,
                            void                   *variable
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
    case CONFIG_VALUE_TYPE_BEGIN_SECTION:
    case CONFIG_VALUE_TYPE_END_SECTION:
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
    case CONFIG_VALUE_TYPE_BEGIN_SECTION:
    case CONFIG_VALUE_TYPE_END_SECTION:
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
  ConfigVariable configVariable;
  uint           z;
  const char     *unit;
  ulong          factor;
  String         s;

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
          z = 0;
          while (   (z < configValueFormat->configValue->integerValue.unitCount)
                 && (((*configVariable.i) % configValueFormat->configValue->integerValue.units[z].factor) != 0)
                )
          {
            z++;
          }
          if (z < configValueFormat->configValue->integerValue.unitCount)
          {
            unit   = configValueFormat->configValue->integerValue.units[z].name;
            factor = configValueFormat->configValue->integerValue.units[z].factor;
          }
          else
          {
            unit   = NULL;
            factor = 0;
          }
        }
        else
        {
          unit   = NULL;
          factor = 0;
        }

        if (factor > 0)
        {
          String_format(line,"%ld%s",(*configVariable.i)/factor,unit);
        }
        else
        {
          String_format(line,"%ld",*configVariable.i);
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
          z = 0;
          while (   (z < configValueFormat->configValue->integer64Value.unitCount)
                 && (((*configVariable.l) % configValueFormat->configValue->integer64Value.units[z].factor) != 0)
                )
          {
            z++;
          }
          if (z < configValueFormat->configValue->integer64Value.unitCount)
          {
            unit   = configValueFormat->configValue->integer64Value.units[z].name;
            factor = configValueFormat->configValue->integer64Value.units[z].factor;
          }
          else
          {
            unit   = NULL;
            factor = 0;
          }
        }
        else
        {
          unit   = NULL;
          factor = 0;
        }

        if (factor > 0)
        {
          String_format(line,"%lld%s",(*configVariable.l)/factor,unit);
        }
        else
        {
          String_format(line,"%lld",*configVariable.l);
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
          z = 0;
          while (   (z < configValueFormat->configValue->doubleValue.unitCount)
                 && (fmod((*configVariable.d),configValueFormat->configValue->doubleValue.units[z].factor) != 0)
                )
          {
            z++;
          }
          if (z < configValueFormat->configValue->doubleValue.unitCount)
          {
            unit   = configValueFormat->configValue->doubleValue.units[z].name;
            factor = configValueFormat->configValue->doubleValue.units[z].factor;
          }
          else
          {
            unit   = NULL;
            factor = 0;
          }
        }
        else
        {
          unit   = NULL;
          factor = 0;
        }

        if (factor > 0)
        {
          String_format(line,"%lf",(*configVariable.d)/factor,unit);
        }
        else
        {
          String_format(line,"%lf",*configVariable.d);
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
        String_format(line,"%s",(*configVariable.b)?"yes":"no");

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
        String_format(line,"%d",configVariable.enumeration);

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
        z = 0;
        while (   (z < configValueFormat->configValue->selectValue.selectCount)
               && ((*configVariable.select) != configValueFormat->configValue->selectValue.select[z].value)
              )
        {
          z++;
        }

        // format value
        String_format(line,"%s",(z < configValueFormat->configValue->selectValue.selectCount)?configValueFormat->configValue->selectValue.select[z].name:"");

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
        for (z = 0; z < configValueFormat->configValue->selectValue.selectCount; z++)
        {
          if ((*configVariable.set) & configValueFormat->configValue->setValue.set[z].value)
          {
            if (String_length(s) > 0L) String_appendChar(s,',');
            String_appendCString(s,configValueFormat->configValue->setValue.set[z].name);
          }
        }

        // format value
        String_format(line,"%s",s);

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
        if (((*configVariable.cString) != NULL) && (strchr(*configVariable.cString,' ') != NULL))
        {
          String_format(line,"%'s",*configVariable.cString);
        }
        else
        {
          String_format(line,"%s",*configVariable.cString);
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
// always '?
//        if (!String_empty(*configVariable.string) && (String_findChar(*configVariable.string,STRING_BEGIN,' ') >= 0))
//        {
          String_format(line,"%'S",*configVariable.string);
//        }
//        else
//        {
//          String_format(line,"%S",*configVariable.string);
//        }

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
      case CONFIG_VALUE_TYPE_BEGIN_SECTION:
      case CONFIG_VALUE_TYPE_END_SECTION:
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

#ifdef __GNUC__
  #pragma GCC pop_options
#endif /* __GNUC__ */

#ifdef __GNUG__
}
#endif

/* end of file */
