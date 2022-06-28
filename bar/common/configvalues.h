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

#include "common/global.h"
#include "strings.h"
#include "stringlists.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/
#define CONFIG_VALUE_INDEX_NONE MAX_UINT
#define CONFIG_VALUE_INDEX_MAX  MAX_UINT

typedef enum
{
  CONFIG_VALUE_OPERATION_INIT,
  CONFIG_VALUE_OPERATION_DONE,
  CONFIG_VALUE_OPERATION_TEMPLATE,  // return template text
  CONFIG_VALUE_OPERATION_COMMENTS,  // return comments
  CONFIG_VALUE_OPERATION_FORMAT     // format line
} ConfigValueOperations;

/***************************** Datatypes ******************************/

/***********************************************************************\
* Name   : ConfigReportFunction
* Purpose: report function
* Input  : errorMessage - message
*          userData     - user data
* Output : -
* Return : TRUE if parsed, otherwise FALSE
* Notes  : -
\***********************************************************************/

typedef void(*ConfigReportFunction)(const char *errorMessage, void *userData);

/***********************************************************************\
* Name   : ConfigParseFunction
* Purpose: config parse function
* Input  : userData         - user data
*          variable         - config variable
*          name             - config name
*          value            - value
*          errorMessage     - error message variable (can be NULL)
*          errorMessageSize - error message size
* Output : -
* Return : TRUE if parsed, otherwise FALSE
* Notes  : -
\***********************************************************************/

// TODO: userData as last parameter
typedef bool(*ConfigParseFunction)(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

/***********************************************************************\
* Name   : ConfigFormatFunction
* Purpose: format config operation
* Input  : formatData - format data
*          operation  - operation code; see ConfigValueOperations
*          data       - operation data
*          userData   - user data
* Output : data - line
* Return : next data or NULL
* Notes  : -
\***********************************************************************/

typedef bool(*ConfigFormatFunction)(void **formatData, ConfigValueOperations operation, void *data, void *userData);

// section data iterator
typedef void* ConfigValueSectionDataIterator;

/***********************************************************************\
* Name   : ConfigSectionIteratorFunction
* Purpose: section iterator operation
* Input  : sectionData - section iterator variable
*          operation   - operation code; see ConfigValueOperations
*          data        - operation data
*          userData    - user data
* Output : data - name
* Return : next data or NULL
* Notes  : -
\***********************************************************************/

typedef void*(*ConfigSectionIteratorFunction)(ConfigValueSectionDataIterator *sectionIterator, ConfigValueOperations operation, void *data, void *userData);

// config value data types
typedef enum
{
  CONFIG_VALUE_TYPE_NONE,

  CONFIG_VALUE_TYPE_SEPARATOR,
  CONFIG_VALUE_TYPE_SPACE,
  CONFIG_VALUE_TYPE_COMMENT,

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
    int                   min,max;                // valid range
    const ConfigValueUnit *units;                 // list with units
  } integerValue;
  struct
  {
    int64                 min,max;                // valid range
    const ConfigValueUnit *units;                 // list with units
  } integer64Value;
  struct
  {
    double                min,max;                // valid range
    const ConfigValueUnit *units;                 // list with units
  } doubleValue;
  struct
  {
  } booleanValue;
  struct
  {
    uint enumerationValue;                        // emumeration value for this enumeration
  } enumValue;
  struct
  {
    const ConfigValueSelect *selects;             // array with select values
  } selectValue;
  struct
  {
    const ConfigValueSet *sets;                   // array with set values
  } setValue;
  struct
  {
  } cStringValue;
  struct
  {
  } stringValue;
  struct
  {
    ConfigParseFunction  parse;                   // parse line
    ConfigFormatFunction format;                  // format line
    void                 *userData;               // user data for parse special
  } specialValue;
  const char *templateText;
  struct
  {
    ConfigParseFunction parse;                    // parse line
    void                *userData;                // user data for parse deprecated
    const char          *newName;                 // new name
    bool                warningFlag;              // TRUE to print warning
  } deprecatedValue;
  struct
  {
    const char *newName;                          // new name
    bool       warningFlag;                       // TRUE to print warning
  } ignoreValue;
  struct
  {
    ConfigSectionIteratorFunction iteratorFunction;  // section iterator
    void                          *userData;      // user data for parse special
  } section;
  struct
  {
    const char *text;
  } separator;
  struct
  {
    const char *text;
  } comment;
} ConfigValue;

/* example

CONFIG_VALUE_INTEGER        (<name>,<variable>,<offset>|-1,<min>,<max>,<units>,<templateText>                   )
CONFIG_VALUE_INTEGER64      (<name>,<variable>,<offset>|-1,<min>,<max>,<units>,<templateText>                   )
CONFIG_VALUE_DOUBLE         (<name>,<variable>,<offset>|-1,,<templateText>                                      )
CONFIG_VALUE_BOOLEAN        (<name>,<variable>,<offset>|-1,,<templateText>                                      )
CONFIG_VALUE_BOOLEAN_YESNO  (<name>,<variable>,<offset>|-1,,<templateText>                                      )
CONFIG_VALUE_ENUM           (<name>,<variable>,<offset>|-1,<value>,<templateText>                               )
CONFIG_VALUE_SELECT         (<name>,<variable>,<offset>|-1,<select> ,<templateText>                             )
CONFIG_VALUE_SET            (<name>,<variable>,<offset>|-1,<set>,<templateText>                                 )
CONFIG_VALUE_CSTRING        (<name>,<variable>,<offset>|-1,,<templateText>                                      )
CONFIG_VALUE_STRING         (<name>,<variable>,<offset>|-1, ,<templateText>                                     )
CONFIG_VALUE_SPECIAL        (<name>,<function>,<offset>|-1,<parse>,<formatInit>,<formatDone>,<format>,<userData>)
CONFIG_VALUE_DEPRECATED     (<name>,<function>,<offset>|-1,<parse>,<userData>,<newName>,warningFlag>            )

CONFIG_VALUE_SEPARATOR      (<text>                                                                             )
CONFIG_VALUE_COMMENT        (<comment>                                                                          )
CONFIG_VALUE_SPACE          (                                                                                   )

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
  CONFIG_VALUE_INTEGER      ("integer", &intValue,     offsetof(X,a),0,0,123,NULL,NULL                  ),
  CONFIG_VALUE_INTEGER      ("unit",    &intValue,     NULL,-1,      0,0,123,COMMAND_LINE_UNITS,NULL    ),

  CONFIG_VALUE_DOUBLE       ("double",  &doubleValue,  NULL,-1,      0.0,-2.0,4.0,NULL                  ),

  CONFIG_VALUE_BOOLEAN      ("bool",    &boolValue,    NULL,-1,      FALSE,NULL                         ),
  CONFIG_VALUE_BOOLEAN_YESNO("bool",    &boolValue,    NULL,-1,      FALSE,NULL                         ),

  CONFIG_VALUE_SELECT       ("type",    &selectValue,  NULL,-1,      CONFIG_VALUE_SELECT_TYPES,NULL     ),

  CONFIG_VALUE_CSTRING      ("string",  &stringValue,  NULL,-1,      "",NULL                            ),
  CONFIG_VALUE_STRING       ("string",  &stringValue,  NULL,-1,      "",NULL                            ),

  CONFIG_VALUE_ENUM         ("e1",      &enumValue,    NULL,-1,      ENUM1,NULL                         ),
  CONFIG_VALUE_ENUM         ("e2",      &enumValue,    NULL,-1,      ENUM2,NULL                         ),
  CONFIG_VALUE_ENUM         ("e3",      &enumValue,    NULL,-1,      ENUM3,NULL                         ),
  CONFIG_VALUE_ENUM         ("e4",      &enumValue,    NULL,-1,      ENUM4,NULL                         ),

  CONFIG_VALUE_SPECIAL      ("special", &specialValue, NULL,-1,      parseSpecial,123,NULL              ),

  CONFIG_VALUE_BOOLEAN      ("flag",    &helpFlag,     NULL,-1,      FALSE,NULL                         ),

  CONFIG_VALUE_DEPRECATED   ("foo",     &foo,               -1,      configValueParseFoo,NULL,"new",TRUE),

  CONFIG_VALUE_SEPARATOR    ("foo"                                                                      ),
  CONFIG_VALUE_COMMENT      ("foo"                                                                      ),
  CONFIG_VALUE_SPACE        (                                                                           ),
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
    {NULL,NULL,NULL},\
    NULL,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  } \
}; \

/***********************************************************************\
* Name   : CONFIG_VALUE_SECTION_ARRAY, CONFIG_STRUCT_VALUE_SECTION_ARRAY
* Purpose: define config value section array
* Input  : name            - name
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          sectionIterator - section iterator function
*          userData        - user data for section iterator function
*          ...             - config values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SECTION_ARRAY(name,variablePointer,offset,sectionIterator,userData,...) \
  { \
    CONFIG_VALUE_TYPE_BEGIN_SECTION,\
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
    {NULL,NULL,NULL},\
    NULL,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {sectionIterator,userData},\
    {NULL},\
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
    {NULL,NULL,NULL},\
    NULL,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_SECTION_ARRAY(name,type,member,sectionIterator,userData,...) \
  CONFIG_VALUE_SECTION_ARRAY(name,NULL,offsetof(type,member),sectionIterator,userData,__VA_ARGS__)

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
*          templateText    - template text for write
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_INTEGER(name,variablePointer,offset,min,max,units,templateText) \
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
    {NULL,NULL,NULL},\
    templateText,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_INTEGER(name,type,member,min,max,units,templateText) \
  CONFIG_VALUE_INTEGER(name,NULL,offsetof(type,member),min,max,units,templateText)

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
*          templateText    - template text for write
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_INTEGER64(name,variablePointer,offset,min,max,units,templateText) \
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
    {NULL,NULL,NULL},\
    templateText,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_INTEGER64(name,type,member,min,max,units,templateText) \
  CONFIG_VALUE_INTEGER64(name,NULL,offsetof(type,member),min,max,units,templateText)

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
*          templateText    - template text for write
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_DOUBLE(name,variablePointer,offset,min,max,units,templateText) \
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
    {NULL,NULL,NULL},\
    templateText,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_DOUBLE(name,type,member,min,max,units,templateText) \
  CONFIG_VALUE_DOUBLE(name,NULL,offsetof(type,member),min,max,units,templateText)

/***********************************************************************\
* Name   : CONFIG_VALUE_BOOLEAN, CONFIG_STRUCT_VALUE_BOOLEAN
* Purpose: define an bool-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          templateText    - template text for write
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_BOOLEAN(name,variablePointer,offset,templateText) \
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
    {NULL,NULL,NULL},\
    templateText,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_BOOLEAN(name,type,member,templateText) \
  CONFIG_VALUE_BOOLEAN(name,NULL,offsetof(type,member),templateText)

/***********************************************************************\
* Name   : CONFIG_VALUE_SELECT, CONFIG_STRUCT_VALUE_SELECT
* Purpose: define a selection from a set of enum-values
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          selects         - selects definition array
*          templateText    - template text for write
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SELECT(name,variablePointer,offset,selects,templateText) \
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
    {NULL,NULL,NULL},\
    templateText,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_SELECT(name,type,member,selects,templateText) \
  CONFIG_VALUE_SELECT(name,NULL,offsetof(type,member),selects,templateText)

/***********************************************************************\
* Name   : CONFIG_VALUE_SET, CONFIG_STRUCT_VALUE_SET
* Purpose: define an set-value (multiple values)
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          set             - set definition array
*          templateText    - template text for write
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SET(name,variablePointer,offset,set,templateText) \
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
    {NULL,NULL,NULL},\
    templateText,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_SET(name,type,member,set,templateText) \
  CONFIG_VALUE_SET(name,NULL,offsetof(type,member),set,templateText)

/***********************************************************************\
* Name   : CONFIG_VALUE_CSTRING, CONFIG_STRUCT_VALUE_CSTRING
* Purpose: define an C-string-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          templateText    - template text for write
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_CSTRING(name,variablePointer,offset,templateText) \
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
    {NULL,NULL,NULL},\
    templateText,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_CSTRING(name,type,member,templateText) \
  CONFIG_VALUE_CSTRING(name,NULL,offsetof(type,member),templateText)

/***********************************************************************\
* Name   : CONFIG_VALUE_STRING, CONFIG_STRUCT_VALUE_STRING
* Purpose: define an string-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          offset          - offset in structure or -1
*          type            - structure type
*          member          - structure memory name
*          templateText    - template text for write
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_STRING(name,variablePointer,offset,templateText) \
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
    {NULL,NULL,NULL},\
    templateText,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_STRING(name,type,member,templateText) \
  CONFIG_VALUE_STRING(name,NULL,offsetof(type,member),templateText)

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

#define CONFIG_VALUE_SPECIAL(name,variablePointer,offset,parse,format,userData) \
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
    {parse,format,userData},\
    NULL,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_SPECIAL(name,type,member,parse,format,userData) \
  CONFIG_VALUE_SPECIAL(name,NULL,offsetof(type,member),parse,format,userData)

/***********************************************************************\
* Name   : CONFIG_VALUE_IGNORE, CONFIG_STRUCT_VALUE_IGNORE
* Purpose: define an string-value
* Input  : name - name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_IGNORE(name,newName,warningFlag) \
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
    {NULL,NULL,NULL},\
    NULL,\
    {NULL,NULL,NULL,FALSE},\
    {newName,warningFlag},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_IGNORE(name,newName,warningFlag) \
  CONFIG_VALUE_IGNORE(name,newName,warningFlag)

/***********************************************************************\
* Name   : CONFIG_VALUE_DEPRECATED, CONFIG_STRUCT_VALUE_DEPRECATED
* Purpose: define an string-value
* Input  : name            - name
*          variablePointer - pointer to variable or NULL
*          type            - structure type
*          member          - structure memory name
*          parse           - parse function
*          userData        - user data for parse function
*          newName         - new name or NULL
*          warningFlag     - TRUE to print warning
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_DEPRECATED(name,variablePointer,offset,parse,userData,newName,warningFlag) \
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
    {NULL,NULL,NULL},\
    NULL,\
    {parse,userData,newName,warningFlag},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }
#define CONFIG_STRUCT_VALUE_DEPRECATED(name,type,member,parse,userData,newName,warningFlag) \
  CONFIG_VALUE_DEPRECATED(name,NULL,offsetof(type,member),parse,userData,newName,warningFlag)

/***********************************************************************\
* Name   : CONFIG_VALUE_SEPARATOR
* Purpose: separator comment
* Input  : text - comment text
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SEPARATOR(text) \
  { \
    CONFIG_VALUE_TYPE_SEPARATOR,\
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
    {NULL,NULL,NULL},\
    NULL,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {text},\
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
    {NULL,NULL,NULL},\
    NULL,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {text}\
  }

/***********************************************************************\
* Name   : CONFIG_VALUE_SPACE
* Purpose: empty line
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CONFIG_VALUE_SPACE() \
  { \
    CONFIG_VALUE_TYPE_SPACE,\
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
    {NULL,NULL,NULL},\
    NULL,\
    {NULL,NULL,NULL,FALSE},\
    {NULL,FALSE},\
    {NULL,NULL},\
    {NULL},\
    {NULL}\
  }

/***********************************************************************\
* Name   : CONFIG_VALUE_ITERATE
* Purpose: iterated over config value array
* Input  : configValues - config values array
*          sectionName  - section name or NULL
*          index        - iteration variable
* Output : -
* Return : -
* Notes  : index will contain all indizes
*          usage:
*            CONFIG_VALUE_ITERATE(configValues,index)
*            {
*              ... = configValues[index]...
*            }
\***********************************************************************/

#define CONFIG_VALUE_ITERATE(configValues,sectionName,index) \
  for ((index) = ConfigValue_firstValueIndex(configValues,sectionName); \
       ((index) != CONFIG_VALUE_INDEX_NONE) && ((index) <= ConfigValue_lastValueIndex(configValues,sectionName)); \
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
* Notes  : index will contain all indizes
*          usage:
*            CONFIG_VALUE_ITERATEX(configValues,index,TRUE)
*            {
*              ... = configValues[index]...
*            }
\***********************************************************************/

#define CONFIG_VALUE_ITERATEX(configValues,sectionName,index,condition) \
  for ((index) = ConfigValue_firstValueIndex(configValues,sectionName); \
       ((index) != CONFIG_VALUE_INDEX_NONE) && ((index) <= ConfigValue_lastValueIndex(configValues,sectionName)) && (condition)); \
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
* Notes  : index will contain all indizes of section
*          usage:
*            CONFIG_VALUE_ITERATE_SECTION(configValues,sectionName,index)
*            {
*              ... = configValues[index]...
*            }
\***********************************************************************/

#define CONFIG_VALUE_ITERATE_SECTION(configValues,sectionName,index) \
  for ((index) = ConfigValue_firstValueIndex(configValues,sectionName); \
       (index) <= ConfigValue_lastValueIndex(configValues,sectionName); \
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
* Notes  : index will contain all indizes of section
*          usage:
*            CONFIG_VALUE_ITERATE_SECTION(configValues,sectionName,index,TRUE)
*            {
*              ... = configValues[index]...
*            }
\***********************************************************************/

#define CONFIG_VALUE_ITERATE_SECTIONX(configValues,sectionName,index,condition) \
  for ((index) = ConfigValue_firstValueIndex(configValues,sectionName); \
       ((index) <= ConfigValue_lastValueIndex(configValues,sectionName)) && (condition); \
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

bool ConfigValue_init(const ConfigValue configValues[]);

/***********************************************************************
* Name   : ConfigValue_done
* Purpose: deinit config values
* Input  : configValues - array with config value specification
* Output : -
* Return : -
* Notes  :
***********************************************************************/

void ConfigValue_done(const ConfigValue configValues[]);

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
* Return : index or CONFIG_VALUE_INDEX_NONE
* Notes  : -
\***********************************************************************/

uint ConfigValue_valueIndex(const ConfigValue configValues[],
                            const char        *sectionName,
                            const char        *name
                           );

/***********************************************************************\
* Name   : ConfigValue_find
* Purpose: find value
* Input  : configValues                   - config values array
*          firstValueIndex,lastValueIndex - first/last value index or
*                                           CONFIG_VALUE_INDEX_NONE
*          name                           - name
* Output : -
* Return : index or CONFIG_VALUE_INDEX_NONE
* Notes  : -
\***********************************************************************/

uint ConfigValue_find(const ConfigValue configValues[],
                      uint              firstValueIndex,
                      uint              lastValueIndex,
                      const char        *name
                     );

/***********************************************************************\
* Name   : ConfigValue_findSection
* Purpose: find section value indizes
* Input  : configValues - config values array
*          sectionName  - section name
* Output : firstValueIndex - first value index (can be NULL)
*          lastValueIndex  - last value index (can be NULL)
* Return : section index or CONFIG_VALUE_INDEX_NONE
* Notes  : -
\***********************************************************************/

uint ConfigValue_findSection(const ConfigValue configValues[],
                             const char        *sectionName,
                             uint              *firstValueIndex,
                             uint              *lastValueIndex
                            );

/***********************************************************************\
* Name   : ConfigValue_firstValueIndex
* Purpose: get first value index
* Input  : configValues - config values array
* Output : -
* Return : first index or CONFIG_VALUE_INDEX_NONE
* Notes  : -
\***********************************************************************/

uint ConfigValue_firstValueIndex(const ConfigValue configValues[],
                                 const char        *sectionName
                                );

/***********************************************************************\
* Name   : ConfigValue_lastValueIndex
* Purpose: get last value index
* Input  : configValues - config values array
* Output : -
* Return : last index or CONFIG_VALUE_INDEX_NONE
* Notes  : -
\***********************************************************************/

uint ConfigValue_lastValueIndex(const ConfigValue configValues[],
                                const char        *sectionName
                               );

/***********************************************************************\
* Name   : ConfigValue_nextValueIndex
* Purpose: get next value index
* Input  : configValues - config values array
*          index        - index
* Output : -
* Return : next index or CONFIG_VALUE_INDEX_NONE
* Notes  : -
\***********************************************************************/

uint ConfigValue_nextValueIndex(const ConfigValue configValues[],
                                uint              index
                               );

/***********************************************************************
* Name   : ConfigValue_parse
* Purpose: parse config value
* Input  : configValue           - config value
*          sectionName           - section name or NULL
*          value                 - value
*          errorReportFunction   - error report function (can be NULL)
*          errorReportUserData   - error report user data
*          warningReportFunction - warning report function (can be NULL)
*          warningReportUserData - warning report user data
* Output : variable    - variable (can be NULL)
*          commentList - comment list (can be NULL)
* Return : TRUE if config value parsed, FALSE on error
* Notes  :
***********************************************************************/

bool ConfigValue_parse(const ConfigValue    *configValue,
                       const char           *sectionName,
                       const char           *value,
                       ConfigReportFunction errorReportFunction,
                       void                 *errorReportUserData,
                       ConfigReportFunction warningReportFunction,
                       void                 *warningReportUserData,
                       void                 *variable,
                       const StringList     *commentList
                      );

/***********************************************************************\
* Name   : ConfigValue_parseDeprecatedBoolean
* Purpose: config value option call back for deprecated boolean value
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool ConfigValue_parseDeprecatedBoolean(void       *userData,
                                        void       *variable,
                                        const char *name,
                                        const char *value,
                                        char       errorMessage[],
                                        uint       errorMessageSize
                                       );

/***********************************************************************\
* Name   : ConfigValue_parseDeprecatedInteger
* Purpose: config value option call back for deprecated integer value
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool ConfigValue_parseDeprecatedInteger(void       *userData,
                                        void       *variable,
                                        const char *name,
                                        const char *value,
                                        char       errorMessage[],
                                        uint       errorMessageSize
                                       );

/***********************************************************************\
* Name   : ConfigValue_parseDeprecatedInteger64
* Purpose: config value option call back for deprecated integer 64bit
*          value
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool ConfigValue_parseDeprecatedInteger64(void       *userData,
                                          void       *variable,
                                          const char *name,
                                          const char *value,
                                          char       errorMessage[],
                                          uint       errorMessageSize
                                         );

/***********************************************************************\
* Name   : ConfigValue_parseDeprecatedString
* Purpose: config value option call back for deprecated string value
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool ConfigValue_parseDeprecatedString(void       *userData,
                                       void       *variable,
                                       const char *name,
                                       const char *value,
                                       char       errorMessage[],
                                       uint       errorMessageSize
                                      );

bool ConfigValue_isCommentLine(const ConfigValue configValues[], ConstString line);

/***********************************************************************\
* Name   : ConfigValue_setComments
* Purpose: set config value comments
* Input  : configValue - config value
*          commentList - comment list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ConfigValue_setComments(const ConfigValue *configValue,
                             const StringList  *commentList
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
* Name   : ConfigValue_getMatchingUnit, ConfigValue_getMatchingUnit64,
*          ConfigValue_getMatchingUnitDouble
* Purpose: get matching unit for number
* Input  : n         - number
*          units     - config value units (for integer, double)
*          unitCount - number of config value units
* Output : -
* Return : unit
* Notes  : -
\***********************************************************************/

ConfigValueUnit ConfigValue_getMatchingUnit(int n, const ConfigValueUnit units[], uint unitCount);
ConfigValueUnit ConfigValue_getMatchingUnit64(int64 n, const ConfigValueUnit units[], uint unitCount);
ConfigValueUnit ConfigValue_getMatchingUnitDouble(double n, const ConfigValueUnit units[], uint unitCount);

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
* Name   : ConfigValue_formatUnitsTemplate
* Purpose: format units template text
* Input  : buffer     - string variable
*          bufferSize - string variable size
*          units      - units array
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

const char *ConfigValue_formatUnitsTemplate(char *buffer, uint bufferSize, const ConfigValueUnit *units);

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
* Name   : ConfigValue_updateConfigFile
* Purpose: i[date config file from template
* Input  : configFileName - config file name
*          configValues   - config values definition
*          variable       - variable or NULL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ConfigValue_writeConfigFile(ConstString       configFileName,
                                   const ConfigValue configValues[],
                                   const void        *variable
                                  );

/***********************************************************************\
* Name   : ConfigValue_listSectionDataIterator
* Purpose: section iterator handler
* Input  : sectionDataIterator - section data iterator variable
*          operation           - operation to execute; see
*                                CONFIG_VALUE_OPERATION_...
*          data                - data variable (can be NULL)
*          userData            - user data
* Output : data - data
* Return : next section element or NULL
* Notes  : -
\***********************************************************************/

void *ConfigValue_listSectionDataIterator(ConfigValueSectionDataIterator *sectionDataIterator,
                                          ConfigValueOperations          operation,
                                          void                           *data,
                                          void                           *userData
                                         );

#ifndef NDEBUG
/***********************************************************************\
* Name   : ConfigValue_debugDumpComments
* Purpose: dump comments list
* Input  : handle - output channel
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ConfigValue_debugDumpComments(FILE *handle);

/***********************************************************************\
* Name   : ConfigValue_debugPrintComments
* Purpose: dump comments list to stdout
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ConfigValue_debugPrintComments(void);

void ConfigValue_debugSHA256(const ConfigValue configValues[], void *buffer, uint bufferLength);
#endif /* not NDEBUG */

#ifdef __GNUG__
}
#endif

#endif /* __CONFIG_VALUE__ */

/* end of file */
