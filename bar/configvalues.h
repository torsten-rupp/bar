/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/configvalues.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: command line options parser
* Systems: all
*
***********************************************************************/

#ifndef __CONFIG_VALUE__
#define __CONFIG_VALUE__

/****************************** Includes ******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "global.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/

#define CMD_LEVEL_ALL    -1
#define CMD_PRIORITY_ANY -1

/***************************** Datatypes ******************************/
typedef enum
{
  CONFIG_VALUE_TYPE_INTEGER,
  CONFIG_VALUE_TYPE_INTEGER64,
  CONFIG_VALUE_TYPE_DOUBLE,
  CONFIG_VALUE_TYPE_BOOLEAN,
  CONFIG_VALUE_TYPE_ENUM,
  CONFIG_VALUE_TYPE_SELECT,
  CONFIG_VALUE_TYPE_STRING,
  CONFIG_VALUE_TYPE_SPECIAL
} ConfigValueTypes;

typedef struct
{
  const char *name;
  uint64     factor;
} ConfigValueUnit;

typedef struct
{
  const char *name;
  int        value;
} ConfigValueSelect;

typedef union
{
  void   *pointer;
  void   **reference;
  int    *n;
  int64  *l;
  double *d;
  bool   *b;
  uint   *enumeration;
  uint   *select;
  char   **string;
  void   *special;
} ConfigVariable;

typedef struct
{
  const char       *name;
  ConfigValueTypes type;
  ConfigVariable   variable;
  int              offset;
  struct
  {
    int                     min,max;              // valid range
    const ConfigValueUnit   *units;               // list with units
    uint                    unitCount;            // number of units
  } integerValue;
  struct
  {
    int64                   min,max;              // valid range
    const ConfigValueUnit   *units;               // list with units
    uint                    unitCount;            // number of units
  } integer64Value;
  struct
  {
    double                  min,max;              // valid range
    const ConfigValueUnit   *units;               // list with units
    uint                    unitCount;            // number of units
  } doubleValue;
  struct
  {
  } booleanValue;
  struct
  {
    uint                    enumerationValue;     // emumeration value for this enumeration
  } enumValue;
  struct
  {
    const ConfigValueSelect *selects;             // list with select values
    uint                    selectCount;          // number of select values
  } selectValue;
  struct
  {
  } stringValue;
  struct
  {
    bool(*parseSpecial)(void *userData, void *variable, const char *name, const char *value);
    void                    *userData;            // user data for parse special
  } specialValue;
} ConfigValue;

/* example

CMD_OPTION_INTEGER        (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,<min>,<max>,<units>,<help text>                )
CMD_OPTION_INTEGER_RANGE  (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,<min>,<max>,<units>,<help text>                )
CMD_OPTION_INTEGER64      (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,<min>,<max>,<units>,<help text>                )
CMD_OPTION_INTEGER64_RANGE(<long name>,<short name>,<level>,<priority>,<variable>,<default value>,<min>,<max>,<units>,<help text>                )
CMD_OPTION_DOUBLE         (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,                    <help text>                )
CMD_OPTION_DOUBLE_RANGE   (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,<min>,<max>,        <help text>                )
CMD_OPTION_BOOLEAN        (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,                    <help text>                )
CMD_OPTION_BOOLEAN_YESNO  (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,                    <help text>                )
CMD_OPTION_ENUM           (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,<value>,            <help text>                )
CMD_OPTION_STRING         (<long name>,<short name>,<level>,<priority>,<variable>,<default value>,                    <help text>,<help argument>)
CMD_OPTION_SPECIAL        (<long name>,<short name>,<level>,<priority>,<function>,<default value>,                    <help text>,<help argument>)

const CommandLineUnit COMMAND_LINE_UNITS[] =
{
  {"k",1024},
  {"m",1024*1024},
  {"g",1024*1024*1024},
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_SELECT_OUTPUTYPE[] =
{
  {"c",   1,"type1"},
  {"h",   2,"type2"},
  {"both",3,"type3"},
};

const CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_INTEGER      ("integer", 'i',0,0,intValue,   0,0,123,NULL                                  "integer value"),
  CMD_OPTION_INTEGER      ("unit",    'u',0,0,intValue,   0,0,123,COMMAND_LINE_UNITS                    "integer value with unit"),
  CMD_OPTION_INTEGER_RANGE("range1",  'r',0,0,intValue,   0,0,123,COMMAND_LINE_UNITS                    "integer value with unit and range"),

  CMD_OPTION_DOUBLE       ("double",  'd',0,0,doubleValue,0.0,-2.0,4.0,                                 "double value"),
  CMD_OPTION_DOUBLE_RANGE ("range2",   0,  0,0,doubleValue,0.0,-2.0,4.0,                                 "double value with range"),

  CMD_OPTION_BOOLEAN_YESNO("bool",    'b',0,0,boolValue,  FALSE,                                        "bool value 1"),

  CMD_OPTION_SELECT       ("type",    't',0,0,outputType, 1,      COMMAND_LINE_OPTIONS_SELECT_OUTPUTYPE,"select value",NULL),

  CMD_OPTION_STRING       ("string",  0,  0,0,stringValue,"",                                           "string value"),

  CMD_OPTION_ENUM         ("e1",      '1',0,0,enumValue,  0,ENUM1,                                      "enum 1"), 
  CMD_OPTION_ENUM         ("e2",      '2',0,0,enumValue,  0,ENUM2,                                      "enum 2"), 
  CMD_OPTION_ENUM         ("e3",      '3',0,0,enumValue,  0,ENUM3,                                      "enum 3"), 
  CMD_OPTION_ENUM         ("e4",      '4',0,0,enumValue,  0,ENUM4,                                      "enum 4"), 

  CMD_OPTION_SPECIAL      ("special", 's',0,1,specialValue,parseSpecial,123,                            "special","abc"), 
  CMD_OPTION_INTEGER      ("extended",'i',1,0,extendValue,0,0,123,NULL                                  "extended integer"),

  CMD_OPTION_BOOLEAN      ("help",    'h',0,0,helpFlag,   FALSE,                                        "output this help"),
};

*/

/***************************** Variables ******************************/

/******************************* Macros *******************************/
#define CONFIG_VALUE_INTEGER(name,variable,offset,min,max,units) \
  {\
    name,\
    CONFIG_VALUE_TYPE_INTEGER,\
    {&variable},\
    offset,\
    {min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0},\
    {},\
    {NULL,NULL}\
  }

#define CONFIG_VALUE_INTEGER64(name,variable,offset,min,max,units) \
  {\
    name,\
    CONFIG_VALUE_TYPE_INTEGER64,\
    {&variable},\
    offset,\
    {0,0,NULL,0},\
    {min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0},\
    {},\
    {NULL,NULL}\
  }

#define CONFIG_VALUE_DOUBLE(name,variable,offset,min,max,units,description) \
  {\
    name,\
    CONFIG_VALUE_TYPE_DOUBLE,\
    {&variable},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {},\
    {0},\
    {NULL,0},\
    {},\
    {NULL,NULL,NULL}\
  }

#define CONFIG_VALUE_BOOLEAN(name,variable,offset) \
  {\
    name,\
    CONFIG_VALUE_TYPE_BOOLEAN,\
    {&variable},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0},\
    {},\
    {NULL,NULL}\
  }
#define CONFIG_VALUE_BOOLEAN_YESNO(name,variable,offset) \
  {\
    name,\
    CONFIG_VALUE_TYPE_BOOLEAN,\
    {&variable},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {TRUE},\
    {0},\
    {NULL,0},\
    {},\
    {NULL,NULL}\
  }

#define CONFIG_VALUE_ENUM(name,variable,offset,value) \
  {\
    name,\
    CONFIG_VALUE_TYPE_ENUM,\
    {&variable},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {value},\
    {NULL,0},\
    {},\
    {NULL,NULL}\
  }

#define CONFIG_VALUE_SELECT(name,variable,offset,selects) \
  {\
    name,\
    CONFIG_VALUE_TYPE_SELECT,\
    {&variable},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {selects,sizeof(selects)/sizeof(CommandLineOptionSelect)},\
    {},\
    {NULL,NULL}\
  }

#define CONFIG_VALUE_STRING(name,variable,offset) \
  {\
    name,\
    CONFIG_VALUE_TYPE_STRING,\
    {&variable},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0},\
    {},\
    {NULL,NULL},\
  }

#define CONFIG_VALUE_SPECIAL(name,variablePointer,offset,parseSpecial,userData) \
  {\
    name,\
    CONFIG_VALUE_TYPE_SPECIAL,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0},\
    {},\
    {0},\
    {NULL,0},\
    {},\
    {parseSpecial,userData}\
  }

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************
* Name   : ConfigFile_init
* Purpose: init command line options with default values
* Input  : commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
* Output : -
* Return : TRUE if command line parsed, FALSE on error
* Notes  :
***********************************************************************/

bool ConfigValue_init(const ConfigValue configValues[],
                      uint              configValueCount
                     );

/***********************************************************************
* Name   : ConfigFile_init
* Purpose: deinit command line options
* Input  : commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
* Output : -
* Return : -
* Notes  :
***********************************************************************/
void ConfigValue_done(const ConfigValue configValues[],
                      uint              configValueCount
                     );

/***********************************************************************
* Name   : ConfigFile_parse
* Purpose: parse config value
* Input  : argv                   - command line arguments
*          argc                   - number of command line arguments
*          commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
*          errorOutputHandle      - error output handle or NULL
*          errorPrefix            - error prefix or NULL
* Output : arguments
*          argumentsCount
* Return : TRUE if command line parsed, FALSE on error
* Notes  :
***********************************************************************/

bool ConfigValue_parse(const char        *name,
                       const char        *value,
                       const ConfigValue configValues[],
                       uint              configValueCount,
                       FILE              *errorOutputHandle,
                       const char        *errorPrefix
                      );

#ifdef __GNUG__
}
#endif

#endif /* __CONFIG_VALUE__ */

/* end of file */
