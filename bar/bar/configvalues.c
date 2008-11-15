/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/bar/configvalues.c,v $
* $Revision: 1.1 $
* $Author: torsten $
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
* Name   : processValue
* Purpose: process single config value
* Input  : configValue       - config value
*          prefix            - option prefix ("-" or "--")
*          name              - option name
*          value             - option value or NULL
*          errorOutputHandle - error output handle or NULL
*          errorPrefix       - error prefix or NULL
* Output : -
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

  assert(configValue != NULL);
  assert(name != NULL);

  switch (configValue->type)
  {
    case CONFIG_VALUE_TYPE_INTEGER:
      {
        uint  i,j,z;
        char  number[128],unit[32];
        ulong factor;
        int   n;

        /* split number, unit */
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

        /* find factor */
        if (unit[0] != '\0')
        {
          if (configValue->integerValue.units != NULL)
          {
            z = 0;
            while (   (z < configValue->integerValue.unitCount)
                   && (strcmp(configValue->integerValue.units[z].name,unit) != 0)
                  )
            {
              z++;
            }
            if (z >= configValue->integerValue.unitCount)
            {
              if (errorOutputHandle != NULL)
              {
                fprintf(errorOutputHandle,
                        "%sInvalid unit in integer value '%s'! Valid units:",
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
            factor = configValue->integerValue.units[z].factor;
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

        /* calculate value */
        n = strtoll(value,NULL,0)*factor;
        if (   (n < configValue->integerValue.min)
            || (n > configValue->integerValue.max)
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

        /* store value */
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.n = (int*)((byte*)variable+configValue->offset);
            (*configVariable.n) = n;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.n = (int*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.n) = n;
            }
          }
        }
        else
        {
          assert(configValue->variable.n != NULL);
          (*configValue->variable.n) = n;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_INTEGER64:
      {
        uint  i,j,z;
        char  number[128],unit[32];
        ulong factor;
        int64 l;

        /* split number, unit */
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

        /* find factor */
        if (unit[0] != '\0')
        {
          if (configValue->integer64Value.units != NULL)
          {
            z = 0;
            while (   (z < configValue->integer64Value.unitCount)
                   && (strcmp(configValue->integer64Value.units[z].name,unit) != 0)
                  )
            {
              z++;
            }
            if (z >= configValue->integer64Value.unitCount)
            {
              if (errorOutputHandle != NULL)
              {
                fprintf(errorOutputHandle,
                        "%sInvalid unit in integer value '%s'! Valid units:",
                        (errorPrefix != NULL)?errorPrefix:"",
                        value
                       );
                for (z = 0; z < configValue->integer64Value.unitCount; z++)
                {
                  fprintf(errorOutputHandle," %s",configValue->integer64Value.units[z].name);
                }
                fprintf(errorOutputHandle,".\n");
              }
              return FALSE;
            }
            factor = configValue->integer64Value.units[z].factor;
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

        /* calculate value */
        l = strtoll(value,NULL,0)*factor;
        if (   (l < configValue->integer64Value.min)
            || (l > configValue->integer64Value.max)
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

        /* store value */
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.l = (int64*)((byte*)variable+configValue->offset);
            (*configVariable.l) = l;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.l = (int64*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.l) = l;
            }
          }
        }
        else
        {
          assert(configValue->variable.l != NULL);
          (*configValue->variable.l) = l;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_DOUBLE:
      {
        uint  i,j,z;
        char  number[128],unit[32];
        ulong factor;
        double d;

        /* split number, unit */
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

        /* find factor */
        if (unit[0] != '\0')
        {
          if (configValue->doubleValue.units != NULL)
          {
            z = 0;
            while (   (z < configValue->doubleValue.unitCount)
                   && (strcmp(configValue->doubleValue.units[z].name,unit) != 0)
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

        /* calculate value */
        d = strtod(value,0);
        if (   (d < configValue->doubleValue.min)
            || (d > configValue->doubleValue.max)
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

        /* store value */
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.d = (double*)((byte*)variable+configValue->offset);
            (*configVariable.d) = d;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.d = (double*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.d) = d;
            }
          }
        }
        else
        {
          assert(configValue->variable.d != NULL);
          (*configValue->variable.d) = d;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_BOOLEAN:
      {
        bool b;

        /* calculate value */
        if      (   (value == NULL)
                 || (strcmp(value,"1") == 0)
                 || (strcmp(value,"true") == 0)
                 || (strcmp(value,"on") == 0)
                 || (strcmp(value,"yes") == 0)
                )
        {
          b = TRUE;
        }
        else if (   (strcmp(value,"0") == 0)
                 || (strcmp(value,"false") == 0)
                 || (strcmp(value,"off") == 0)
                 || (strcmp(value,"no") == 0)
                )
        {
          b = FALSE;
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

        /* store value */
        if (configValue->offset >= 0)
        {
          if (variable != NULL)
          {
            configVariable.b = (bool*)((byte*)variable+configValue->offset);
            (*configVariable.b) = b;
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.b = (bool*)((byte*)(*configValue->variable.reference)+configValue->offset);
              (*configVariable.b) = b;
            }
          }
        }
        else
        {
          assert(configValue->variable.b != NULL);
          (*configValue->variable.b) = b;
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

        /* find select value */
        z = 0;
        while (   (z < configValue->selectValue.selectCount)
               && (strcmp(configValue->selectValue.select[z].name,value) != 0)
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

        /* store value */
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
        uint  i,j,z;
        char  setName[128];

        /* find and store set values */
        assert(configValue->variable.set != NULL);
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

          /* skip , */
          if (value[i] == ',') i++;

          if (setName[0] != '\0')
          {
            /* find value */
            z = 0;
            while ((z < configValue->setValue.setCount) && (strcmp(configValue->setValue.set[z].name,setName) != 0))
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

            /* store value */
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
            assert(*configVariable.string != NULL);
            String_setCString(*configVariable.string,value);
          }
          else
          {
            assert(configValue->variable.reference != NULL);
            if ((*configValue->variable.reference) != NULL)
            {
              configVariable.string = (String*)((byte*)(*configValue->variable.reference)+configValue->offset);
              if ((*configVariable.string) == NULL) (*configVariable.string) = String_new();
              assert(*configVariable.string != NULL);
              String_setCString(*configVariable.string,value);
            }
          }
        }
        else
        {
          assert(configValue->variable.string != NULL);
          if ((*configValue->variable.string) == NULL) (*configValue->variable.string) = String_new();
          assert(*configValue->variable.string != NULL);
          String_setCString(*configValue->variable.string,value);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_SPECIAL:
      /* store value */
      if (configValue->offset >= 0)
      {
        if (variable != NULL)
        {
          if (!configValue->specialValue.parse(configValue->specialValue.userData,
                                               (byte*)variable+configValue->offset,
                                               configValue->name,
                                               value
                                              )
             )
          {
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
                                                 value
                                                )
               )
            {
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
                                             value
                                            )
           )
        {
          return FALSE;
        }
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  return TRUE;
}

/*---------------------------------------------------------------------*/

bool ConfigValue_init(const ConfigValue configValues[],
                      uint              configValueCount
                     )
{
  uint i;

  assert(configValues != NULL);

  for (i = 0; i < configValueCount; i++)
  {
    switch (configValues[i].type)
    {
      case CONFIG_VALUE_TYPE_INTEGER:
        assert(configValues[i].variable.n != NULL);
        break;
      case CONFIG_VALUE_TYPE_INTEGER64:
        assert(configValues[i].variable.l != NULL);
        break;
      case CONFIG_VALUE_TYPE_DOUBLE:
        assert(configValues[i].variable.d != NULL);
        break;
      case CONFIG_VALUE_TYPE_BOOLEAN:
        assert(configValues[i].variable.b != NULL);
        break;
      case CONFIG_VALUE_TYPE_ENUM:
        assert(configValues[i].variable.enumeration != NULL);
        break;
      case CONFIG_VALUE_TYPE_SELECT:
        assert(configValues[i].variable.select != NULL);
        break;
      case CONFIG_VALUE_TYPE_SET:
        assert(configValues[i].variable.set != NULL);
        break;
      case CONFIG_VALUE_TYPE_CSTRING:
        assert(configValues[i].variable.cString != NULL);
        break;
      case CONFIG_VALUE_TYPE_STRING:
        assert(configValues[i].variable.string != NULL);
        break;
      case CONFIG_VALUE_TYPE_SPECIAL:
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

void ConfigValue_done(const ConfigValue configValues[],
                      uint              configValueCount
                     )
{
  uint i;

  assert(configValues != NULL);

  for (i = 0; i < configValueCount; i++)
  {
    switch (configValues[i].type)
    {
      case CONFIG_VALUE_TYPE_INTEGER:
        break;
      case CONFIG_VALUE_TYPE_INTEGER64:
        break;
      case CONFIG_VALUE_TYPE_DOUBLE:
        break;
      case CONFIG_VALUE_TYPE_BOOLEAN:
        break;
      case CONFIG_VALUE_TYPE_ENUM:
        break;
      case CONFIG_VALUE_TYPE_SELECT:
        break;
      case CONFIG_VALUE_TYPE_SET:
        break;
      case CONFIG_VALUE_TYPE_CSTRING:
        assert(configValues[i].variable.cString != NULL);
        if ((*configValues[i].variable.cString) != NULL)
        {
          free(*configValues[i].variable.cString);
        }
        break;
      case CONFIG_VALUE_TYPE_STRING:
        assert(configValues[i].variable.string != NULL);
        if ((*configValues[i].variable.string) != NULL)
        {
          String_delete(*configValues[i].variable.string);
        }
        break;
      case CONFIG_VALUE_TYPE_SPECIAL:
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
  }
}

bool ConfigValue_parse(const char        *name,
                       const char        *value,
                       const ConfigValue configValues[],
                       uint              configValueCount,
                       FILE              *errorOutputHandle,
                       const char        *errorPrefix,
                       void              *variable
                      )
{
  uint i;

  assert(name != NULL);
  assert(configValues != NULL);

  /* find config value */
  i = 0;
  while ((i < configValueCount) && (strcmp(configValues[i].name,name) != 0))
  {
    i++;
  }
  if (i >= configValueCount)
  {
    return FALSE;
  }

  /* process value */
  if (!processValue(&configValues[i],name,value,errorOutputHandle,errorPrefix,variable))
  {
    return FALSE;
  }

  return TRUE;
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
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
}

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
        /* get value */
        if (configValueFormat->configValue->offset >= 0)
        {
          if (configValueFormat->variable != NULL)
          {
            configVariable.n = (int*)((byte*)configValueFormat->variable+configValueFormat->configValue->offset);
          }
          else
          {
            assert(configValueFormat->configValue->variable.reference != NULL);
            configVariable.n = (int*)((byte*)(*configValueFormat->configValue->variable.reference)+configValueFormat->configValue->offset);
          }
        }
        else
        {
          assert(configValueFormat->configValue->variable.n != NULL);
          configVariable.n = configValueFormat->configValue->variable.n;
        }

        /* find usable unit */
        if ((configValueFormat->configValue->integerValue.units != NULL) && ((*configVariable.n) != 0))
        {
          z = 0;
          while (   (z < configValueFormat->configValue->integerValue.unitCount)
                 && (((*configVariable.n) % configValueFormat->configValue->integerValue.units[z].factor) != 0)
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
          String_format(line,"%ld%s",(*configVariable.n)/factor,unit);
        }
        else
        {
          String_format(line,"%ld",*configVariable.n);
        }

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_INTEGER64:
        /* get value */
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

        /* find usable unit */
        if ((configValueFormat->configValue->integer64Value.units != NULL) && ((*configVariable.l) != 0L))
        {
          z = 0;
          while (   (z < configValueFormat->configValue->integer64Value.unitCount)
                 && (((*configVariable.n) % configValueFormat->configValue->integer64Value.units[z].factor) != 0)
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
        /* get value */
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

        /* find usable unit */
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
        /* get value */
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

        /* format value */
        String_format(line,"%s",(*configVariable.b)?"yes":"no");

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_ENUM:
        /* get value */
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

        /* format value */
        String_format(line,"%d",configVariable.enumeration);

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_SELECT:
        /* get value */
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

        /* format value */
        String_format(line,"%s",(z < configValueFormat->configValue->selectValue.selectCount)?configValueFormat->configValue->selectValue.select[z].name:"");

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_SET:
        /* get value */
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
            if (String_length(s) > 0) String_appendChar(s,',');
            String_appendCString(s,configValueFormat->configValue->setValue.set[z].name);
          }
        }

        /* format value */
        String_format(line,"%s",s);

        /* free resources */
        String_delete(s);

        configValueFormat->endOfDataFlag = TRUE;
        break;
      case CONFIG_VALUE_TYPE_CSTRING:
        /* get value */
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
          assert(configValueFormat->configValue->variable.n != NULL);
          configVariable.cString = configValueFormat->configValue->variable.cString;
        }

        /* format value */
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
        /* get value */
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
          assert(configValueFormat->configValue->variable.n != NULL);
          configVariable.string = configValueFormat->configValue->variable.string;
        }

        /* format value */
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

#ifdef __GNUG__
}
#endif

/* end of file */
