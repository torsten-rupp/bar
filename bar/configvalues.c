/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/configvalues.c,v $
* $Revision: 1.2 $
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
                        const char        *errorPrefix
                       )
{
  uint           i,n;
  char           number[128],unit[32];
  ulong          factor;
  ConfigVariable configVariable;

  assert(configValue != NULL);
  assert(name != NULL);

  switch (configValue->type)
  {
    case CONFIG_VALUE_TYPE_INTEGER:
      {
        int n;

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
          if (configValue->integerValue.units != NULL)
          {
            i = 0;
            while ((i < configValue->integerValue.unitCount) && (strcmp(configValue->integerValue.units[i].name,unit) != 0))
            {
              i++;
            }
            if (i >= configValue->integerValue.unitCount)
            {
              if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                     "%sInvalid unit in integer value '%s'!\n",
                                                     (errorPrefix != NULL)?errorPrefix:"",
                                                     value
                                                    );
              return FALSE;
            }
            factor = configValue->integerValue.units[i].factor;
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

        if (configValue->offset >= 0)
        {
          assert(configValue->variable.pointer != NULL);
          if ((*configValue->variable.reference) != NULL)
          {
            configVariable.n = (int*)((byte*)(*configValue->variable.reference)+configValue->offset);
            (*configVariable.n) = n;
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
        int64 l;

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
          if (configValue->integer64Value.units != NULL)
          {
            i = 0;
            while ((i < configValue->integer64Value.unitCount) && (strcmp(configValue->integer64Value.units[i].name,unit) != 0))
            {
              i++;
            }
            if (i >= configValue->integer64Value.unitCount)
            {
              if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                     "%sInvalid unit in integer value '%s'!\n",
                                                     (errorPrefix != NULL)?errorPrefix:"",
                                                     value
                                                    );
              return FALSE;
            }
            factor = configValue->integer64Value.units[i].factor;
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

        if (configValue->offset >= 0)
        {
          assert(configValue->variable.reference != NULL);
          if ((*configValue->variable.reference) != NULL)
          {
            configVariable.l = (int64*)((byte*)(*configValue->variable.reference)+configValue->offset);
            (*configVariable.l) = l;
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
        double d;

        assert(configValue->variable.d != NULL);

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
          if (configValue->doubleValue.units != NULL)
          {
            i = 0;
            while ((i < configValue->doubleValue.unitCount) && (strcmp(configValue->doubleValue.units[i].name,unit) != 0))
            {
              i++;
            }
            if (i >= configValue->doubleValue.unitCount)
            {
              if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                     "%sInvalid unit in integer value '%s'!\n",
                                                     (errorPrefix != NULL)?errorPrefix:"",
                                                     value
                                                    );
              return FALSE;
            }
            factor = configValue->doubleValue.units[i].factor;
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

        if (configValue->offset >= 0)
        {
          assert(configValue->variable.reference != NULL);
          if ((*configValue->variable.reference) != NULL)
          {
            configVariable.d = (double*)((byte*)(*configValue->variable.reference)+configValue->offset);
            (*configVariable.d) = d;
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

        assert(configValue->variable.b != NULL);
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

        if (configValue->offset >= 0)
        {
          assert(configValue->variable.reference != NULL);
          if ((*configValue->variable.reference) != NULL)
          {
            configVariable.b = (bool*)((byte*)(*configValue->variable.reference)+configValue->offset);
            (*configVariable.b) = b;
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
        assert(configValue->variable.enumeration != NULL);

        if (configValue->offset >= 0)
        {
          assert(configValue->variable.reference != NULL);
          if ((*configValue->variable.reference) != NULL)
          {
            configVariable.enumeration = (int*)((byte*)(*configValue->variable.reference)+configValue->offset);
            (*configVariable.enumeration) = configValue->enumValue.enumerationValue;
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
        assert(configValue->variable.select != NULL);
        i = 0;
        while ((i < configValue->selectValue.selectCount) && (strcmp(configValue->selectValue.selects[i].name,value) != 0))
        {
          i++;
        }
        if (i >= configValue->selectValue.selectCount)
        {
          if (errorOutputHandle != NULL) fprintf(errorOutputHandle,
                                                 "%sUnknown value '%s' for config value '%s'!\n",
                                                 (errorPrefix != NULL)?errorPrefix:"",
                                                 value,
                                                 name
                                                );
          return FALSE;
        }

        if (configValue->offset >= 0)
        {
          assert(configValue->variable.reference != NULL);
          if ((*configValue->variable.reference) != NULL)
          {
            configVariable.enumeration = (int*)((byte*)(*configValue->variable.reference)+configValue->offset);
            (*configVariable.enumeration) = configValue->selectValue.selects[i].value;
          }
        }
        else
        {
          assert(configValue->variable.enumeration != NULL);
          (*configValue->variable.enumeration) = configValue->selectValue.selects[i].value;
        }
      }
      break;
    case CONFIG_VALUE_TYPE_STRING:
      {
        assert(configValue->variable.string != NULL);
        if ((*configValue->variable.string) != NULL)
        {
          free(*configValue->variable.string);
        }

        if (configValue->offset >= 0)
        {
          assert(configValue->variable.reference != NULL);
          if ((*configValue->variable.reference) != NULL)
          {
            configVariable.string = (char**)((byte*)(*configValue->variable.reference)+configValue->offset);
            (*configVariable.string) = strdup(value);
          }
        }
        else
        {
          assert(configValue->variable.enumeration != NULL);
          (*configValue->variable.string) = strdup(value);
        }
      }
      break;
    case CONFIG_VALUE_TYPE_SPECIAL:
        if (configValue->offset >= 0)
        {
          assert(configValue->variable.reference != NULL);
          if ((*configValue->variable.reference) != NULL)
          {
            if (!configValue->specialValue.parseSpecial(configValue->specialValue.userData,
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
        else
        {
          assert(configValue->variable.special != NULL);
          if (!configValue->specialValue.parseSpecial(configValue->specialValue.userData,
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
      case CONFIG_VALUE_TYPE_STRING:
        assert(configValues[i].variable.string != NULL);
        break;
      case CONFIG_VALUE_TYPE_SPECIAL:
        break;
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
      case CONFIG_VALUE_TYPE_STRING:
        assert(configValues[i].variable.string != NULL);
        if ((*configValues[i].variable.string) != NULL)
        {
          free(*configValues[i].variable.string);
        }
        break;
      case CONFIG_VALUE_TYPE_SPECIAL:
        break;
    }
  }
}

bool ConfigValue_parse(const char        *name,
                       const char        *value,
                       const ConfigValue configValues[],
                       uint              configValueCount,
                       FILE              *errorOutputHandle,
                       const char        *errorPrefix
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
  if (!processValue(&configValues[i],name,value,errorOutputHandle,errorPrefix))
  {
    return FALSE;
  }

  return TRUE;
}

#ifdef __GNUG__
}
#endif

/* end of file */
