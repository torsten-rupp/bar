/**********************************************************************
*
* $Revision$
* $Date$
* $Author$
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
#include "stringlists.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/

/***************************** Datatypes ******************************/

// config value data types
typedef enum
{
  CONFIG_VALUE_TYPE_NONE,

  CONFIG_VALUE_TYPE_INTEGER,
  CONFIG_VALUE_TYPE_INTEGER64,
  CONFIG_VALUE_TYPE_DOUBLE,
  CONFIG_VALUE_TYPE_BOOLEAN,
  CONFIG_VALUE_TYPE_ENUM,
  CONFIG_VALUE_TYPE_SELECT,
  CONFIG_VALUE_TYPE_SET,
  CONFIG_VALUE_TYPE_CSTRING,
  CONFIG_VALUE_TYPE_STRING,
  CONFIG_VALUE_TYPE_SPECIAL,

  CONFIG_VALUE_TYPE_IGNORE,
  CONFIG_VALUE_TYPE_DEPRECATED,

  CONFIG_VALUE_TYPE_BEGIN_SECTION,
  CONFIG_VALUE_TYPE_END_SECTION,

  CONFIG_VALUE_TYPE_COMMENT,

  CONFIG_VALUE_TYPE_END
} ConfigValueTypes;

// config value unit
typedef struct
{
  const char *name;
  uint64     factor;
} ConfigValueUnit;

// config value select value
typedef struct
{
  const char *name;
  uint       value;
} ConfigValueSelect;

// config value set value
typedef struct
{
  const char *name;
  uint       value;
} ConfigValueSet;

// config value definition
typedef struct
{
  ConfigValueTypes type;                          // type of config value
  const char       *name;                         // name of config value or section name
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
    void       *deprecated;
  }                variable;                      // variable
  int              offset;                        // offset in struct or -1
  struct
  {
    int                     min,max;              // valid range
    const ConfigValueUnit   *units;               // list with units
  } integerValue;
  struct
  {
    int64                   min,max;              // valid range
    const ConfigValueUnit   *units;               // list with units
  } integer64Value;
  struct
  {
    double                  min,max;              // valid range
    const ConfigValueUnit   *units;               // list with units
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
    const ConfigValueSelect *selects;             // array with select values
  } selectValue;
  struct
  {
    const ConfigValueSet    *sets;                // array with set values
  } setValue;
  struct
  {
  } cStringValue;
  struct
  {
  } stringValue;
  struct
  {
    bool(*parse)(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
    void(*formatInit)(void **formatUserData, void *userData, void *variable);
    void(*formatDone)(void **formatUserData, void *userData);
    bool(*format)(void **formatUserData, void *userData, String line);
    void *userData;                               // user data for parse special
  } specialValue;
  struct
  {
    bool(*parse)(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
    void       *userData;                         // user data for parse deprecated
    const char *newName;                          // new name
  } deprecatedValue;
  struct
  {
    const char *text;
  } comment;
} ConfigValue;

/* example

CONFIG_VALUE_INTEGER        (<name>,<variable>,<offset>|-1,<min>,<max>,<units>                                  )
CONFIG_VALUE_INTEGER64      (<name>,<variable>,<offset>|-1,<min>,<max>,<units>                                  )
CONFIG_VALUE_DOUBLE         (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_BOOLEAN        (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_BOOLEAN_YESNO  (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_ENUM           (<name>,<variable>,<offset>|-1,<value>                                              )
CONFIG_VALUE_SELECT         (<name>,<variable>,<offset>|-1,<select>                                             )
CONFIG_VALUE_SET            (<name>,<variable>,<offset>|-1,<set>                                                )
CONFIG_VALUE_CSTRING        (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_STRING         (<name>,<variable>,<offset>|-1,                                                     )
CONFIG_VALUE_SPECIAL        (<name>,<function>,<offset>|-1,<parse>,<formatInit>,<formatDone>,<format>,<userData>)
CONFIG_VALUE_DEPRECATED     (<name>,<function>,<offset>|-1,<parse>,<userData>,<newName>                         )
CONFIG_VALUE_COMMENT        (<comment>                                                                          )

const ConfigValueUnit COMMAND_LINE_UNITS[] = CONFIG_VALUE_UNIT_ARRAY
(
  {"k",1024},
  {"m",1024*1024},
  {"g",1024*1024*1024},
);

const ConfigValueSelect CONFIG_VALUE_SELECT_TYPES[] = CONFIG_VALUE_SELECT_ARRAY
(
  {"c",   1,"type1"},
  {"h",   2,"type2"},
  {"both",3,"type3"},
);

const ConfigValue CONFIG_VALUES[] =
{
  CONFIG_VALUE_INTEGER      ("integer", &intValue,     offsetof(X,a),0,0,123,NULL,                   ),
  CONFIG_VALUE_INTEGER      ("unit",    &intValue,     NULL,-1,      0,0,123,COMMAND_LINE_UNITS      ),

  CONFIG_VALUE_DOUBLE       ("double",  &doubleValue,  NULL,-1,      0.0,-2.0,4.0,                   ),

  CONFIG_VALUE_BOOLEAN_YESNO("bool",    &boolValue,    NULL,-1,      FALSE,                          ),

  CONFIG_VALUE_SELECT       ("type",    &selectValue,  NULL,-1,      CONFIG_VALUE_SELECT_TYPES       ),

  CONFIG_VALUE_CSTRING      ("string",  &stringValue,  NULL,-1,      "",                             ),
  CONFIG_VALUE_STRING       ("string",  &stringValue,  NULL,-1,      "",                             ),

  CONFIG_VALUE_ENUM         ("e1",      &enumValue,    NULL,-1,      ENUM1,                          ),
  CONFIG_VALUE_ENUM         ("e2",      &enumValue,    NULL,-1,      ENUM2,                          ),
  CONFIG_VALUE_ENUM         ("e3",      &enumValue,    NULL,-1,      ENUM3,                          ),
  CONFIG_VALUE_ENUM         ("e4",      &enumValue,    NULL,-1,      ENUM4,                          ),

  CONFIG_VALUE_SPECIAL      ("special", &specialValue, NULL,-1,      parseSpecial,123,               ),

  CONFIG_VALUE_BOOLEAN      ("flag",    &helpFlag,     NULL,-1,      FALSE,                          ),

  CONFIG_VALUE_DEPRECATED   ("foo",     &foo,               -1,      configValueParseFoo,NULL,  "new"),

  CONFIG_VALUE_COMMENT      ("comment"                                                               ),
};

const ConfigValue CONFIG_STRUCT_VALUES[] =
{
  CONFIG_VALUE_INTEGER      ("integer", X,intValue,     0,0,123,NULL,              ),
  CONFIG_VALUE_INTEGER      ("unit",    X,intValue      0,0,123,COMMAND_LINE_UNITS ),
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

// config format value modes
typedef enum
{
  CONFIG_VALUE_FORMAT_MODE_VALUE,                 // format value only
  CONFIG_VALUE_FORMAT_MODE_LINE,                  // format line
} ConfigValueFormatModes;

// configu value format info
typedef struct
{
  void                   *formatUserData;         // user data for special value call back
  void                   *userData;
  const ConfigValue      *configValue;            // config value to format
  const void             *variable;               // config value variable
  ConfigValueFormatModes mode;
  bool                   endOfDataFlag;           // TRUE iff no more data
} ConfigValueFormat;

/***************************** Variables ******************************/

/******************************* Macros *******************************/

/***********************************************************************\
* Name   : CONFIG_VALUE_UNIT_ARRAY
* Purpose: define unit array
* Input  : ... - unit values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_UNIT_ARRAY(...) \
{ \
  __VA_ARGS__ \
  {NULL,0} \
}; \

/***********************************************************************\
* Name   : CONFIG_VALUE_SELECT_ARRAY
* Purpose: define select array
* Input  : ... - select values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SELECT_ARRAY(...) \
{ \
  __VA_ARGS__ \
  {NULL,0} \
}

/***********************************************************************\
* Name   : CONFIG_VALUE_SET_ARRAY
* Purpose: define set array
* Input  : ... - set values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SET_ARRAY(...) \
{ \
  __VA_ARGS__ \
  {NULL,0} \
}

/***********************************************************************\
* Name   : CONFIG_VALUE_ARRAY
* Purpose: define config value array
* Input  : ... - config values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_ARRAY(...) \
{ \
  __VA_ARGS__ \
  { \
    CONFIG_VALUE_TYPE_END,\
    NULL,\
    {NULL},\
    0,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  } \
}; \

/***********************************************************************\
* Name   : CONFIG_VALUE_SECTION_ARRAY
* Purpose: define config value section array
* Input  : name   - name
*          offset - offset in structure or -1
*          ...    - config values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SECTION_ARRAY(name,offset,...) \
  { \
    CONFIG_VALUE_TYPE_BEGIN_SECTION,\
    name,\
    {NULL},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }, \
  __VA_ARGS__ \
  { \
    CONFIG_VALUE_TYPE_END_SECTION,\
    NULL,\
    {NULL},\
    0,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }

/***********************************************************************\
* Name   : CONFIG_VALUE_INTEGER, CONFIG_STRUCT_VALUE_INTEGER
* Purpose: define an int-value, support units for number
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          min,max         - min./max. value
*          units           - units definition array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_INTEGER(name,variablePointer,offset,min,max,units) \
  { \
    CONFIG_VALUE_TYPE_INTEGER,\
    name,\
    {variablePointer},\
    offset,\
    {min,max,units},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL}, \
    {NULL},\
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_INTEGER(name,type,member,min,max,units) \
  CONFIG_VALUE_INTEGER(name,NULL,offsetof(type,member),min,max,units)

/***********************************************************************\
* Name   : CONFIG_VALUE_INTEGER64, CONFIG_STRUCT_VALUE_INTEGER64
* Purpose: define an int64-value, support units for number
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          min,max         - min./max. value
*          units           - units definition array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_INTEGER64(name,variablePointer,offset,min,max,units) \
  { \
    CONFIG_VALUE_TYPE_INTEGER64,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {min,max,units},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_INTEGER64(name,type,member,min,max,units) \
  CONFIG_VALUE_INTEGER64(name,NULL,offsetof(type,member),min,max,units)

/***********************************************************************\
* Name   : CONFIG_VALUE_DOUBLE, CONFIG_STRUCT_VALUE_DOUBLE
* Purpose: define an double-value, support units for number
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          min,max         - min./max. value
*          units           - units definition array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_DOUBLE(name,variablePointer,offset,min,max,units) \
  { \
    CONFIG_VALUE_TYPE_DOUBLE,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {min,max,units},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_DOUBLE(name,type,member,min,max,units) \
  CONFIG_VALUE_DOUBLE(name,NULL,offsetof(type,member),min,max,units)

/***********************************************************************\
* Name   : CONFIG_VALUE_BOOLEAN, CONFIG_STRUCT_VALUE_BOOLEAN
* Purpose: define an bool-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_BOOLEAN(name,variablePointer,offset) \
  { \
    CONFIG_VALUE_TYPE_BOOLEAN,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_BOOLEAN(name,type,member) \
  CONFIG_VALUE_BOOLEAN(name,NULL,offsetof(type,member))

/***********************************************************************\
* Name   : CONFIG_VALUE_BOOLEAN_YESNO, CONFIG_STRUCT_VALUE_BOOLEAN_YESNO
* Purpose: define an bool-value with yes/no
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_BOOLEAN_YESNO(name,variablePointer,offset) \
  { \
    CONFIG_VALUE_TYPE_BOOLEAN,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {TRUE},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_BOOLEAN_YESNO(name,variablePointer,offset) \
  CONFIG_VALUE_BOOLEAN_YESNO(name,NULL,offsetof(type,member))

/***********************************************************************\
* Name   : CONFIG_VALUE_ENUM, CONFIG_STRUCT_VALUE_ENUM
* Purpose: define an enum-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          value           - enum value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_ENUM(name,variablePointer,offset,value) \
  { \
    CONFIG_VALUE_TYPE_ENUM,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {value},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_ENUM(name,type,member,value) \
  CONFIG_VALUE_ENUM(name,NULL,offsetof(type,member),value)

/***********************************************************************\
* Name   : CONFIG_VALUE_SELECT, CONFIG_STRUCT_VALUE_SELECT
* Purpose: define an enum-value as selection
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          selects         - selects definition array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SELECT(name,variablePointer,offset,selects) \
  { \
    CONFIG_VALUE_TYPE_SELECT,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {selects},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_SELECT(name,type,member,selects) \
  CONFIG_VALUE_SELECT(name,NULL,offsetof(type,member),selects)

/***********************************************************************\
* Name   : CONFIG_VALUE_SET, CONFIG_STRUCT_VALUE_SET
* Purpose: define an set-value (multiple values)
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          set             - set definition array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SET(name,variablePointer,offset,set) \
  { \
    CONFIG_VALUE_TYPE_SET,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL}, \
    {set},\
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_SET(name,type,member,set) \
  CONFIG_VALUE_SET(name,NULL,offsetof(type,member),set)

/***********************************************************************\
* Name   : CONFIG_VALUE_CSTRING, CONFIG_STRUCT_VALUE_CSTRING
* Purpose: define an C-string-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_CSTRING(name,variablePointer,offset) \
  { \
    CONFIG_VALUE_TYPE_CSTRING,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_CSTRING(name,type,member) \
  CONFIG_VALUE_CSTRING(name,NULL,offsetof(type,member))

/***********************************************************************\
* Name   : CONFIG_VALUE_STRING, CONFIG_STRUCT_VALUE_STRING
* Purpose: define an string-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_STRING(name,variablePointer,offset) \
  { \
    CONFIG_VALUE_TYPE_STRING,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_STRING(name,type,member) \
  CONFIG_VALUE_STRING(name,NULL,offsetof(type,member))

/***********************************************************************\
* Name   : CONFIG_VALUE_SPECIAL, CONFIG_STRUCT_VALUE_SPECIAL
* Purpose: define an specal-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          parse           - parse function
*          formatInit      - format init function
*          formatDone      - format done function
*          format          - format function
*          userData        - user data for parse/format functions
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SPECIAL(name,variablePointer,offset,parse,formatInit,formatDone,format,userData) \
  { \
    CONFIG_VALUE_TYPE_SPECIAL,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {parse,formatInit,formatDone,format,userData},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_SPECIAL(name,type,member,parse,formatInit,formatDone,format,userData) \
  CONFIG_VALUE_SPECIAL(name,NULL,offsetof(type,member),parse,formatInit,formatDone,format,userData)

/***********************************************************************\
* Name   : CONFIG_VALUE_IGNORE, CONFIG_STRUCT_VALUE_IGNORE
* Purpose: define an string-value
* Input  : name - name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_IGNORE(name) \
  { \
    CONFIG_VALUE_TYPE_IGNORE,\
    name,\
    {NULL},\
    0,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_IGNORE(name) \
  CONFIG_VALUE_IGNORE(name)

/***********************************************************************\
* Name   : CONFIG_VALUE_DEPRECATED, CONFIG_STRUCT_VALUE_DEPRECATED
* Purpose: define an string-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          parse           - parse function
*          userData        - user data for parse/format functions
*          newName         - new name or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_DEPRECATED(name,variablePointer,offset,parse,userData,newName) \
  { \
    CONFIG_VALUE_TYPE_DEPRECATED,\
    name,\
    {variablePointer},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {parse,userData,newName},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_DEPRECATED(name,type,member,parse,userData,newName) \
  CONFIG_VALUE_DEPRECATED(name,NULL,offsetof(type,member),parse,userData,newName)

/***********************************************************************\
* Name   : CONFIG_VALUE_BEGIN_SECTION, CONFIG_VALUE_END_SECTION
* Purpose: begin/end value section [<name>...]
* Input  : name   - name
*          offset - offset in structure or -1
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_BEGIN_SECTION(name,offset) \
  { \
    CONFIG_VALUE_TYPE_BEGIN_SECTION,\
    name,\
    {NULL},\
    offset,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }

#define CONFIG_VALUE_END_SECTION() \
  { \
    CONFIG_VALUE_TYPE_END_SECTION,\
    NULL,\
    {NULL},\
    0,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {NULL}\
  }

/***********************************************************************\
* Name   : CONFIG_VALUE_COMMENT
* Purpose: comment
* Input  : text - comment text
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_COMMENT(text) \
  { \
    CONFIG_VALUE_TYPE_COMMENT,\
    NULL,\
    {NULL},\
    -1,\
    {0,0,NULL},\
    {0LL,0LL,NULL},\
    {0.0,0.0,NULL},\
    {},\
    {0},\
    {NULL},\
    {NULL}, \
    {},\
    {},\
    {NULL,NULL,NULL,NULL,NULL},\
    {NULL,NULL,NULL},\
    {text}\
  }

/***********************************************************************\
* Name   : CONFIG_VALUE_ITERATE
* Purpose: iterated over config value array
* Input  : configValues - config values array
*          sectionName  - section name or NULL
*          index        - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            CONFIG_VALUE_ITERATE(configValues,index)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define CONFIG_VALUE_ITERATE(configValues,sectionName,index) \
  for ((index) = ConfigValue_firstValueIndex(configValues,sectionName); \
       (index != -1) && ((index) <= ConfigValue_lastValueIndex(configValues,sectionName)); \
       (index) = ConfigValue_nextValueIndex(configValues,index) \
      )

/***********************************************************************\
* Name   : CONFIG_VALUE_ITERATEX
* Purpose: iterated over config value array
* Input  : configValues - config values array
*          sectionName  - section name or NULL
*          index        - iteration variable
*          condition    - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            CONFIG_VALUE_ITERATEX(configValues,index,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define CONFIG_VALUE_ITERATEX(configValues,sectionName,index,condition) \
  for ((index) = ConfigValue_firstValueIndex(configValues,sectionName); \
       (index != -1) && ((index) <= ConfigValue_lastValueIndex(configValues,sectionName)) && (condition)); \
       (index) = ConfigValue_nextValueIndex(configValues,index) \
      )

/***********************************************************************\
* Name   : CONFIG_VALUE_ITERATE_SECTION
* Purpose: iterated over config section value array
* Input  : configValues - config values array
*          sectionName  - section name
*          index        - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            CONFIG_VALUE_ITERATE_SECTION(configValues,sectionName,index)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define CONFIG_VALUE_ITERATE_SECTION(configValues,sectionName,index) \
  for ((index) = ConfigValue_firstValueIndex(configValues,sectionName); \
       (index) <= ConfigValue_lastValueIndex(configValues,index); \
       (index) = ConfigValue_nextValueIndex(configValues,index) \
      )

/***********************************************************************\
* Name   : CONFIG_VALUE_ITERATE_SECTIONX
* Purpose: iterated over config section value array
* Input  : configValues - config values array
*          sectionName  - section name
*          index        - iteration variable
*          condition    - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            CONFIG_VALUE_ITERATE_SECTION(configValues,sectionName,index,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define CONFIG_VALUE_ITERATE_SECTIONX(configValues,sectionName,index,condition) \
  for ((index) = ConfigValue_firstValueIndex(configValues,sectionName); \
       ((index) <= ConfigValue_lastValueIndex(configValues,index)) && (condition); \
       (index) = ConfigValue_nextValueIndex(configValues,index) \
      )

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************
* Name   : ConfigValue_init
* Purpose: init config values
* Input  : configValues - array with config value specification
* Output : -
* Return : TRUE if command line parsed, FALSE on error
* Notes  :
***********************************************************************/

bool ConfigValue_init(ConfigValue configValues[]);

/***********************************************************************
* Name   : ConfigValue_done
* Purpose: deinit config values
* Input  : configValues - array with config value specification
* Output : -
* Return : -
* Notes  :
***********************************************************************/

void ConfigValue_done(ConfigValue configValues[]);

/***********************************************************************\
* Name   : ConfigValue_isValue
* Purpose: check if config value
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline bool ConfigValue_isValue(const ConfigValue configValue)
{
  return    (configValue.type == CONFIG_VALUE_TYPE_INTEGER)
         || (configValue.type == CONFIG_VALUE_TYPE_INTEGER64)
         || (configValue.type == CONFIG_VALUE_TYPE_DOUBLE)
         || (configValue.type == CONFIG_VALUE_TYPE_BOOLEAN)
         || (configValue.type == CONFIG_VALUE_TYPE_ENUM)
         || (configValue.type == CONFIG_VALUE_TYPE_SELECT)
         || (configValue.type == CONFIG_VALUE_TYPE_SET)
         || (configValue.type == CONFIG_VALUE_TYPE_CSTRING)
         || (configValue.type == CONFIG_VALUE_TYPE_STRING)
         || (configValue.type == CONFIG_VALUE_TYPE_SPECIAL);
}

/***********************************************************************\
* Name   : ConfigValue_isSection
* Purpose: check if section
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline bool ConfigValue_isSection(const ConfigValue configValue)
{
  return    (configValue.type == CONFIG_VALUE_TYPE_BEGIN_SECTION)
         || (configValue.type == CONFIG_VALUE_TYPE_END_SECTION);
}

/***********************************************************************\
* Name   : ConfigValue_valueIndex
* Purpose: get value index
* Input  : configValues - config values array
*          sectionName  - section name
*          name         - name
* Output : -
* Return : index or -1
* Notes  : -
\***********************************************************************/

int ConfigValue_valueIndex(const ConfigValue configValues[],
                           const char        *sectionName,
                           const char        *name
                          );

/***********************************************************************\
* Name   : ConfigValue_firstValueIndex
* Purpose: get first value index
* Input  : configValues - config values array
* Output : -
* Return : first index or -1
* Notes  : -
\***********************************************************************/

int ConfigValue_firstValueIndex(const ConfigValue configValues[],
                                const char        *sectionName
                               );

/***********************************************************************\
* Name   : ConfigValue_lastValueIndex
* Purpose: get last value index
* Input  : configValues - config values array
* Output : -
* Return : last index or -1
* Notes  : -
\***********************************************************************/

int ConfigValue_lastValueIndex(const ConfigValue configValues[],
                               const char        *sectionName
                              );

/***********************************************************************\
* Name   : ConfigValue_nextValueIndex
* Purpose: get next value index
* Input  : configValues - config values array
*          index        - index
* Output : -
* Return : next index or -1
* Notes  : -
\***********************************************************************/

int ConfigValue_nextValueIndex(const ConfigValue configValues[],
                               int               index
                              );

/***********************************************************************
* Name   : ConfigValue_parse
* Purpose: parse config value
* Input  : name          - config value name
*          value         - config value
*          configValues  - array with config value specification
*          sectionName   - section name or NULL
*          outputHandle  - error/warning output handle or NULL
*          errorPrefix   - error prefix or NULL
*          warningPrefix - warning prefix or NULL
* Output : variable - variable
* Return : TRUE if config value parsed, FALSE on error
* Notes  :
***********************************************************************/

bool ConfigValue_parse(const char        *name,
                       const char        *value,
                       const ConfigValue configValues[],
                       const char        *sectionName,
                       FILE              *outputHandle,
                       const char        *errorPrefix,
                       const char        *warningPrefix,
                       void              *variable
                      );

/***********************************************************************\
* Name   : ConfigValue_getIntegerValue
* Purpose: get integer value
* Input  : value  - value variable
*          string - string
*          units  - units array or NULL
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

bool ConfigValue_getIntegerValue(int                   *value,
                                 const char            *string,
                                 const ConfigValueUnit *units
                                );

/***********************************************************************\
* Name   : ConfigValue_getInteger64Value
* Purpose: get integer value
* Input  : value  - value variable
*          string - string
*          units  - units array or NULL
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

bool ConfigValue_getInteger64Value(int64                 *value,
                                   const char            *string,
                                   const ConfigValueUnit *units
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
                            const void             *variable
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

/***********************************************************************\
* Name   : ConfigValue_selectToString
* Purpose: get select string
* Input  : selects       - select name/value array
*          value         - value
*          defaultString - default string or NULL
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

const char *ConfigValue_selectToString(const ConfigValueSelect selects[],
                                       uint                    value,
                                       const char              *defaultString
                                      );

/***********************************************************************\
* Name   : ConfigValue_readConfigFileLines
* Purpose: read config file lines
* Input  : configFileName  - config file name
*          configLinesList - line list variable
* Output : configLinesList - line list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ConfigValue_readConfigFileLines(ConstString configFileName, StringList *configLinesList);

/***********************************************************************\
* Name   : ConfigValue_writeConfigFileLines
* Purpose: write config file lines
* Input  : configFileName  - config file name
*          configLinesList - line list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ConfigValue_writeConfigFileLines(ConstString configFileName, const StringList *configLinesList);

/***********************************************************************\
* Name   : ConfigValue_deleteEntries
* Purpose: delete all entries with given name in config line list
* Input  : stringList - file string list to modify
*          section    - name of section or NULL
*          name       - name of value
* Output : -
* Return : next entry in string list or NULL
* Notes  : -
\***********************************************************************/

StringNode *ConfigValue_deleteEntries(StringList *stringList,
                                      const char *section,
                                      const char *name
                                     );

/***********************************************************************\
* Name   : ConfigValue_deleteSections
* Purpose: delete all sections with given name in config line list
* Input  : stringList - file string list to modify
*          section    - name of section
* Output : -
* Return : next entry in string list or NULL
* Notes  : -
\***********************************************************************/

StringNode *ConfigValue_deleteSections(StringList *stringList,
                                       const char *section
                                      );

#ifdef __GNUG__
}
#endif

#endif /* __CONFIG_VALUE__ */

/* end of file */
