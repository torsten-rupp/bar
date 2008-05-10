/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/configvalues.h,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: config file entry parser
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
#include "strings.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/

/***************************** Datatypes ******************************/

/* config value data types */
typedef enum
{
  CONFIG_VALUE_TYPE_INTEGER,
  CONFIG_VALUE_TYPE_INTEGER64,
  CONFIG_VALUE_TYPE_DOUBLE,
  CONFIG_VALUE_TYPE_BOOLEAN,
  CONFIG_VALUE_TYPE_ENUM,
  CONFIG_VALUE_TYPE_SELECT,
  CONFIG_VALUE_TYPE_SET,
  CONFIG_VALUE_TYPE_CSTRING,
  CONFIG_VALUE_TYPE_STRING,
  CONFIG_VALUE_TYPE_SPECIAL
} ConfigValueTypes;

/* configu value unit */
typedef struct
{
  const char *name;
  uint64     factor;
} ConfigValueUnit;

/* configu value select value */
typedef struct
{
  const char *name;
  int        value;
} ConfigValueSelect;

/* configu value set value */
typedef struct
{
  const char *name;
  int        value;
} ConfigValueSet;

/* configu value variable */
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
  ulong  *set;
  char   **cString;
  String *string;
  void   *special;
} ConfigVariable;

/* configu value definition */
typedef struct
{
  const char       *name;                         // name of config value
  ConfigValueTypes type;                          // type of config value
  ConfigVariable   variable;                      // variable or NULL
  int              offset;                        // offset in struct or -1
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
    const ConfigValueSelect *select;              // array with select values
    uint                    selectCount;          // number of select values
  } selectValue;
  struct
  {
    const ConfigValueSet    *set;                 // array with set values
    uint                    setCount;             // number of set values
  } setValue;
  struct
  {
  } cStringValue;
  struct
  {
  } stringValue;
  struct
  {
    bool(*parse)(void *userData, void *variable, const char *name, const char *value);
    void(*formatInit)(void **formatUserData, void *userData, void *variable);
    void(*formatDone)(void **formatUserData, void *userData);
    bool(*format)(void **formatUserData, void *userData, String line);
    void                    *userData;            // user data for parse special
  } specialValue;
} ConfigValue;

/* example

CONFIG_VALUE_INTEGER        (<name>,<variable>,<offset>|-1,<min>,<max>,<units>                                  )
CONFIG_VALUE_INTEGER_RANGE  (<name>,<variable>,<offset>|-1,<min>,<max>,<units>                                  )
CONFIG_VALUE_INTEGER64      (<name>,<variable>,<offset>|-1,<min>,<max>,<units>                                  )
CONFIG_VALUE_INTEGER64_RANGE(<name>,<variable>,<offset>|-1,<min>,<max>,<units>                                  )
CONFIG_VALUE_DOUBLE         (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_DOUBLE_RANGE   (<name>,<variable>,<offset>|-1,<min>,<max>,<units>                                  )
CONFIG_VALUE_BOOLEAN        (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_BOOLEAN_YESNO  (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_ENUM           (<name>,<variable>,<offset>|-1,<value>                                              )
CONFIG_VALUE_SELECT         (<name>,<variable>,<offset>|-1,<select>                                             )
CONFIG_VALUE_SET            (<name>,<variable>,<offset>|-1,<set>                                                )
CONFIG_VALUE_CSTRING        (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_STRING         (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_SPECIAL        (<name>,<function>,<offset>|-1,<parse>,<formatInit>,<formatDone>,<format>,<userData>)

const ConfigValueUnit COMMAND_LINE_UNITS[] =
{
  {"k",1024},
  {"m",1024*1024},
  {"g",1024*1024*1024},
};

const ConfigValueSelect CONFIG_VALUE_SELECT_TYPES[] =
{
  {"c",   1,"type1"},
  {"h",   2,"type2"},
  {"both",3,"type3"},
};

const ConfigValue CONFIG_VALUES[] =
{
  CONFIG_VALUE_INTEGER      ("integer", &intValue,     offsetof(X,a),0,0,123,NULL,              ),
  CONFIG_VALUE_INTEGER      ("unit",    &intValue,     NULL,-1,      0,0,123,COMMAND_LINE_UNITS ),
  CONFIG_VALUE_INTEGER_RANGE("range1",  &intValue,     offsetof(X,b),0,0,123,COMMAND_LINE_UNITS ),

  CONFIG_VALUE_DOUBLE       ("double",  &doubleValue,  NULL,-1,      0.0,-2.0,4.0,              ),
  CONFIG_VALUE_DOUBLE_RANGE ("range2",  &doubleValue,  NULL,-1,      0.0,-2.0,4.0,              ),

  CONFIG_VALUE_BOOLEAN_YESNO("bool",    &boolValue,    NULL,-1,      FALSE,                     ),

  CONFIG_VALUE_SELECT       ("type",    &selectValue,  NULL,-1,      CONFIG_VALUE_SELECT_TYPES  ),

  CONFIG_VALUE_CSTRING      ("string",  &stringValue,  NULL,-1,      "",                        ),
  CONFIG_VALUE_STRING       ("string",  &stringValue,  NULL,-1,      "",                        ),

  CONFIG_VALUE_ENUM         ("e1",      &enumValue,    NULL,-1,      ENUM1,                     ), 
  CONFIG_VALUE_ENUM         ("e2",      &enumValue,    NULL,-1,      ENUM2,                     ), 
  CONFIG_VALUE_ENUM         ("e3",      &enumValue,    NULL,-1,      ENUM3,                     ), 
  CONFIG_VALUE_ENUM         ("e4",      &enumValue,    NULL,-1,      ENUM4,                     ), 

  CONFIG_VALUE_SPECIAL      ("special", &specialValue, NULL,-1,      parseSpecial,123,          ), 

  CONFIG_VALUE_BOOLEAN      ("flag",    &helpFlag,     NULL,-1,      FALSE,                     ),
};

const ConfigValue CONFIG_STRUCT_VALUES[] =
{
  CONFIG_VALUE_INTEGER      ("integer", X,intValue,     0,0,123,NULL,              ),
  CONFIG_VALUE_INTEGER      ("unit",    X,intValue      0,0,123,COMMAND_LINE_UNITS ),
  CONFIG_VALUE_INTEGER_RANGE("range1",  X,intValue,     0,0,123,COMMAND_LINE_UNITS ),
};

or

typedef struct
{
  int a;
  ...
} XY;

const ConfigValue CONFIG_VALUES[] =
{
  CONFIG_STRUCT_VALUE_INTEGER      ("integer", XY,a,NULL),
};

*/

typedef enum
{
  CONFIG_VALUE_FORMAT_MODE_VALUE,                 // format value only
  CONFIG_VALUE_FORMAT_MODE_LINE,                  // format line
} ConfigValueFormatModes;

/* configu value format info */
typedef struct
{
  void                   *formatUserData;         // user data for special value call back
  void                   *userData;
  const ConfigValue      *configValue;            // config value to format
  void                   *variable;               // config value variable
  ConfigValueFormatModes mode;
  bool                   endOfDataFlag;           // TRUE iff no more data
} ConfigValueFormat;

/***************************** Variables ******************************/

/******************************* Macros *******************************/
#define CONFIG_VALUE_INTEGER(name,variablePointer,offset,min,max,units) \
  { \
    name,\
    CONFIG_VALUE_TYPE_INTEGER,\
    {variablePointer},\
    offset,\
    {min,max,units,sizeof(units)/sizeof(ConfigValueUnit)},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0}, \
    {NULL,0},\
    {},\
    {},\
    {NULL,NULL}\
  }
#define CONFIG_STRUCT_VALUE_INTEGER(name,type,member,min,max,units) \
  CONFIG_VALUE_INTEGER(name,NULL,offsetof(type,member),min,max,units)

#define CONFIG_VALUE_INTEGER64(name,variablePointer,offset,min,max,units) \
  { \
    name,\
    CONFIG_VALUE_TYPE_INTEGER64,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {min,max,units,sizeof(units)/sizeof(ConfigValueUnit)},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0},\
    {NULL,0}, \
    {},\
    {},\
    {NULL,NULL}\
  }
#define CONFIG_STRUCT_VALUE_INTEGER64(name,type,member,min,max,units) \
  CONFIG_VALUE_INTEGER64(name,NULL,offsetof(type,member),min,max,units)

#define CONFIG_VALUE_DOUBLE(name,variablePointer,offset,min,max,units) \
  { \
    name,\
    CONFIG_VALUE_TYPE_DOUBLE,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {min,max,units,sizeof(units)/sizeof(ConfigValueUnit)},\
    {},\
    {0},\
    {NULL,0},\
    {NULL,0}, \
    {},\
    {},\
    {NULL,NULL,NULL}\
  }
#define CONFIG_STRUCT_VALUE_DOUBLE(name,type,member,min,max,units) \
  CONFIG_VALUE_DOUBLE(name,NULL,offsetof(type,member),min,max,units)

#define CONFIG_VALUE_BOOLEAN(name,variablePointer,offset) \
  { \
    name,\
    CONFIG_VALUE_TYPE_BOOLEAN,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0},\
    {NULL,0}, \
    {},\
    {},\
    {NULL,NULL}\
  }
#define CONFIG_STRUCT_VALUE_BOOLEAN(name,type,member) \
  CONFIG_VALUE_BOOLEAN(name,NULL,offsetof(type,member))

#define CONFIG_VALUE_BOOLEAN_YESNO(name,variablePointer,offset) \
  { \
    name,\
    CONFIG_VALUE_TYPE_BOOLEAN,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {TRUE},\
    {0},\
    {NULL,0},\
    {NULL,0}, \
    {},\
    {},\
    {NULL,NULL}\
  }
#define CONFIG_STRUCT_VALUE_BOOLEAN_YESNO(name,variablePointer,offset) \
  CONFIG_VALUE_BOOLEAN_YESNO(name,NULL,offsetof(type,member))

#define CONFIG_VALUE_ENUM(name,type,member,value) \
  { \
    name,\
    CONFIG_VALUE_TYPE_ENUM,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {value},\
    {NULL,0},\
    {NULL,0}, \
    {},\
    {},\
    {NULL,NULL}\
  }
#define CONFIG_STRUCT_VALUE_ENUM(name,type,member,value) \
  CONFIG_VALUE_ENUM(name,NULL,offsetof(type,member),value)

#define CONFIG_VALUE_SELECT(name,variablePointer,offset,selects) \
  { \
    name,\
    CONFIG_VALUE_TYPE_SELECT,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {selects,sizeof(selects)/sizeof(ConfigValueSelect)},\
    {NULL,0}, \
    {},\
    {},\
    {NULL,NULL}\
  }
#define CONFIG_STRUCT_VALUE_SELECT(name,type,member,selects) \
  CONFIG_VALUE_SELECT(name,NULL,offsetof(type,member),selects)

#define CONFIG_VALUE_SET(name,variablePointer,offset,set) \
  { \
    name,\
    CONFIG_VALUE_TYPE_SET,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0}, \
    {set,sizeof(set)/sizeof(ConfigValueSet)},\
    {},\
    {},\
    {NULL,NULL}\
  }
#define CONFIG_STRUCT_VALUE_SET(name,type,member,set) \
  CONFIG_VALUE_SET(name,NULL,offsetof(type,member),set)

#define CONFIG_VALUE_CSTRING(name,variablePointer,offset) \
  { \
    name,\
    CONFIG_VALUE_TYPE_CSTRING,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0},\
    {NULL,0}, \
    {},\
    {},\
    {NULL,NULL},\
  }
#define CONFIG_STRUCT_VALUE_CSTRING(name,type,member) \
  CONFIG_VALUE_CSTRING(name,NULL,offsetof(type,member))

#define CONFIG_VALUE_STRING(name,variablePointer,offset) \
  { \
    name,\
    CONFIG_VALUE_TYPE_STRING,\
    {variablePointer},\
    offset,\
    {0,0,NULL,0},\
    {0,0,NULL,0},\
    {0.0,0.0,NULL,0},\
    {},\
    {0},\
    {NULL,0},\
    {NULL,0}, \
    {},\
    {},\
    {NULL,NULL},\
  }
#define CONFIG_STRUCT_VALUE_STRING(name,type,member) \
  CONFIG_VALUE_STRING(name,NULL,offsetof(type,member))

#define CONFIG_VALUE_SPECIAL(name,variablePointer,offset,parse,formatInit,formatDone,format,userData) \
  { \
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
    {NULL,0}, \
    {},\
    {},\
    {parse,formatInit,formatDone,format,userData}\
  }
#define CONFIG_STRUCT_VALUE_SPECIAL(name,type,member,parse,formatInit,formatDone,format,userData) \
  CONFIG_VALUE_SPECIAL(name,NULL,offsetof(type,member),parse,formatInit,formatDone,format,userData)

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************
* Name   : ConfigValue_init
* Purpose: init config values
* Input  : configValues     - array with config value specification
*          configValueCount - size of config value specification array
* Output : -
* Return : TRUE if command line parsed, FALSE on error
* Notes  :
***********************************************************************/

bool ConfigValue_init(const ConfigValue configValues[],
                      uint              configValueCount
                     );

/***********************************************************************
* Name   : ConfigValue_done
* Purpose: deinit config values
* Input  : configValues     - array with config value specification
*          configValueCount - size of config value specification array
* Output : -
* Return : -
* Notes  :
***********************************************************************/
void ConfigValue_done(const ConfigValue configValues[],
                      uint              configValueCount
                     );

/***********************************************************************
* Name   : ConfigValue_parse
* Purpose: parse config value
* Input  : name              - config value name
*          value             - config value
*          configValues      - array with config value specification
*          configValueCount  - size of config value specification array
*          errorOutputHandle - error output handle or NULL
*          errorPrefix       - error prefix or NULL
* Output : variable - variable
* Return : TRUE if config value parsed, FALSE on error
* Notes  :
***********************************************************************/

bool ConfigValue_parse(const char        *name,
                       const char        *value,
                       const ConfigValue configValues[],
                       uint              configValueCount,
                       FILE              *errorOutputHandle,
                       const char        *errorPrefix,
                       void              *variable
                      );


/***********************************************************************\
* Name   : ConfigValue_formatInit
* Purpose: initialize config valule format
* Input  : configValue - config value definition
*          mode        - format mode; see CONFIG_VALUE_FORMAT_MODE_*
*          variable    - config value variable
* Output : configValueFormat - config format info
* Return : -
* Notes  : -
\***********************************************************************/

void ConfigValue_formatInit(ConfigValueFormat      *configValueFormat,
                            const ConfigValue      *configValue,
                            ConfigValueFormatModes mode,
                            void                   *variable
                           );

/***********************************************************************\
* Name   : ConfigValue_formatDone
* Purpose: done config value format
* Input  : configValueFormat - config format info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ConfigValue_formatDone(ConfigValueFormat *configValueFormat);

/***********************************************************************\
* Name   : ConfigValue_format
* Purpose: format next config value
* Input  : configValueFormat - config format info
*          line              - line variable
* Output : line - line
* Return : TRUE if config value formated, FALSE for no more data
* Notes  : -
\***********************************************************************/

bool ConfigValue_format(ConfigValueFormat *configValueFormat,
                        String            line
                       );

#ifdef __GNUG__
}
#endif

#endif /* __CONFIG_VALUE__ */

/* end of file */
