/**********************************************************************
*
* $Revision$
* $Date$
* $Author$
* Contents: command line options parser
* Systems: all
*
***********************************************************************/

#ifndef __CMD_OPTIONS__
#define __CMD_OPTIONS__

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

#define CMD_HELP_LEVEL_ALL -1
#define CMD_PRIORITY_ANY   MAX_UINT

/***************************** Datatypes ******************************/

// command option value data types
typedef enum
{
  CMD_OPTION_TYPE_INTEGER,
  CMD_OPTION_TYPE_INTEGER64,
  CMD_OPTION_TYPE_DOUBLE,
  CMD_OPTION_TYPE_BOOLEAN,
  CMD_OPTION_TYPE_INCREMENT,
  CMD_OPTION_TYPE_ENUM,
  CMD_OPTION_TYPE_SELECT,
  CMD_OPTION_TYPE_SET,
  CMD_OPTION_TYPE_CSTRING,
  CMD_OPTION_TYPE_STRING,
  CMD_OPTION_TYPE_SPECIAL,
  CMD_OPTION_TYPE_DEPRECATED
} CommandLineOptionTypes;

// command option unit
typedef struct
{
  const char *name;
  uint64     factor;
} CommandLineUnit;

// command option select value
typedef struct
{
  const char *name;
  uint       value;
  const char *description;
} CommandLineOptionSelect;

// command option value set
typedef struct
{
  const char *name;
  uint       value;
  const char *description;
} CommandLineOptionSet;

// command option value
typedef struct CommandLineOption
{
  const char             *name;
  char                   shortName;
  unsigned int           helpLevel;
  unsigned int           priority;
  CommandLineOptionTypes type;
  union
  {
    void   *pointer;

    int    *i;
    int64  *l;
    double *d;
    bool   *b;
    uint   *increment;
    uint   *enumeration;
    uint   *select;
    ulong  *set;
    char   **cString;
    String *string;
    void   *special;
    void   *deprecated;
  } variable;
  bool *isSetVariable;
  struct
  {
    int    i;
    int64  l;
    double d;
    bool   b;
    uint   increment;
    union
    {
      uint  enumeration;
      uint  select;
      ulong set;
    };
    union
    {
      const void *pointer;

      char       *cString;
      String     string;
      void       *special;
      void       *deprecated;
    };
  } defaultValue;
  struct
  {
    bool                          rangeFlag;            // TRUE iff range should be printed in help
    int                           min,max;              // valid range
    const CommandLineUnit         *units;               // array with units
    const char                    *descriptionArgument; // optional description text argument
  } integerOption;
  struct
  {
    bool                          rangeFlag;            // TRUE iff range should be printed in help
    int64                         min,max;              // valid range
    const CommandLineUnit         *units;               // array with units
    const char                    *descriptionArgument; // optional description text argument
  } integer64Option;
  struct
  {
    bool                          rangeFlag;            // TRUE iff range should be printed in help
    double                        min,max;              // valid range
    const CommandLineUnit         *units;               // array with units
    const char                    *descriptionArgument; // optional description text argument
  } doubleOption;
  struct
  {
    bool                          yesnoFlag;            // TRUE iff yes/no should be printed in help
  } booleanOption;
  struct
  {
    bool                          rangeFlag;            // TRUE iff range should be printed in help
    int                           min,max;              // valid range
    const char                    *descriptionArgument; // optional description text argument
  } incrementOption;
  struct
  {
    uint                          enumerationValue;     // emumeration value for this enumeration
  } enumOption;
  struct
  {
    const CommandLineOptionSelect *selects;             // array with select values
  } selectOption;
  struct
  {
    const CommandLineOptionSet    *sets;                // array with set values
  } setOption;
  struct
  {
    const char                    *descriptionArgument; // optional description text argument
  } stringOption;
  struct
  {
    bool(*parseSpecial)(void       *userData,
                        void       *variable,
                        const char *option,
                        const char *value,
                        const void *defaultValue,
                        char       *errorMessage,       // must be NUL-terminated!
                        uint       errorMessageSize
                       );
    void                          *userData;            // user data for parse special
    uint                          argumentCount;        // argument count
    const char                    *descriptionArgument; // optional description text argument
  } specialOption;
  struct
  {
    bool(*parseDeprecated)(void       *userData,
                           void       *variable,
                           const char *option,
                           const char *value,
                           const void *defaultValue,
                           char       *errorMessage,       // must be NUL-terminated!
                           uint       errorMessageSize
                          );
    void                          *userData;            // user data for parse special
    uint                          argumentCount;        // argument count
    const char                    *newOptionName;       // new option name
  } deprecatedOption;
  const char             *description;
} CommandLineOption;

/* example

CMD_OPTION_INTEGER        (<long name>,<short name>,<help level>,<priority>,<variable>,<min>,<max>,<units>,         <description>                       )
CMD_OPTION_INTEGER_RANGE  (<long name>,<short name>,<help level>,<priority>,<variable>,<min>,<max>,<units>,         <description>                       )
CMD_OPTION_INTEGER64      (<long name>,<short name>,<help level>,<priority>,<variable>,<min>,<max>,<units>,         <description>                       )
CMD_OPTION_INTEGER64_RANGE(<long name>,<short name>,<help level>,<priority>,<variable>,<min>,<max>,<units>,         <description>                       )
CMD_OPTION_DOUBLE         (<long name>,<short name>,<help level>,<priority>,<variable>,                             <description>                       )
CMD_OPTION_DOUBLE_RANGE   (<long name>,<short name>,<help level>,<priority>,<variable>,<min>,<max>,                 <description>                       )
CMD_OPTION_BOOLEAN        (<long name>,<short name>,<help level>,<priority>,<variable>,                             <description>                       )
CMD_OPTION_BOOLEAN_YESNO  (<long name>,<short name>,<help level>,<priority>,<variable>,                             <description>                       )
CMD_OPTION_INCREMENT      (<long name>,<short name>,<help level>,<priority>,<variable>,                             <description>                       )
CMD_OPTION_ENUM           (<long name>,<short name>,<help level>,<priority>,<variable>,<value>,                     <description>                       )
CMD_OPTION_SELECT         (<long name>,<short name>,<help level>,<priority>,<variable>,<selects>,                   <description>                       )
CMD_OPTION_SET            (<long name>,<short name>,<help level>,<priority>,<variable>,<set>,                       <description>                       )
CMD_OPTION_CSTRING        (<long name>,<short name>,<help level>,<priority>,<variable>,                             <description>,<description argument>)
CMD_OPTION_STRING         (<long name>,<short name>,<help level>,<priority>,<variable>,                             <description>,<description argument>)
CMD_OPTION_SPECIAL        (<long name>,<short name>,<help level>,<priority>,<function>,<user data>,<argument count>,<description>,<description argument>)
CMD_OPTION_DEPRECATED     (<long name>,<short name>,<help level>,<priority>,<function>,<user data>,<argument count>,<description>,<new option name>     )

const CommandLineUnit COMMAND_LINE_UNITS[] =
{
  {"k",1024},
  {"m",1024*1024},
  {"g",1024*1024*1024},
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_SELECT_TYPES[] =
{
  {"c",   1,"type1"},
  {"h",   2,"type2"},
  {"both",3,"type3"},
};

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_SET_TYPES[] =
{
  {"s",1,"type1"},
  {"l",2,"type2"},
  {"x",3,"type3"},
};

const CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_INTEGER      ("integer",  'i',0,0,intValue,   0,0,123,NULL                                  "integer value"),
  CMD_OPTION_INTEGER      ("unit",     'u',0,0,intValue,   0,0,123,COMMAND_LINE_UNITS                    "integer value with unit"),
  CMD_OPTION_INTEGER_RANGE("range1",   'r',0,0,intValue,   0,0,123,COMMAND_LINE_UNITS                    "integer value with unit and range"),

  CMD_OPTION_DOUBLE       ("double",   'd',0,0,doubleValue,0.0,-2.0,4.0,                                 "double value"),
  CMD_OPTION_DOUBLE_RANGE ("range2",    0,  0,0,doubleValue,0.0,-2.0,4.0,                                "double value with range"),

  CMD_OPTION_BOOLEAN_YESNO("bool",     'b',0,0,boolValue,  FALSE,                                        "bool value 1"),

  CMD_OPTION_SELECT       ("type",     't',0,0,outputType, 1,      COMMAND_LINE_OPTIONS_SELECT_TYPES,    "select value",NULL),
  CMD_OPTION_SET          ("set",      0,  0,0,setType,    0,      COMMAND_LINE_OPTIONS_SET_TYPES,       "set value",NULL),

  CMD_OPTION_CSTRING      ("string",   0,  0,0,cStringValue,"",                                          "string value"),
  CMD_OPTION_STRING       ("string",   0,  0,0,stringValue,"",                                           "string value"),

  CMD_OPTION_INCREMENT    ("increment",'v',0,0,incrementValue,0,2                                        "increment"),

  CMD_OPTION_ENUM         ("e1",       '1',0,0,enumValue,  ENUM1,ENUM1,                                  "enum 1"),
  CMD_OPTION_ENUM         ("e2",       '2',0,0,enumValue,  ENUM1,ENUM2,                                  "enum 2"),
  CMD_OPTION_ENUM         ("e3",       '3',0,0,enumValue,  ENUM1,ENUM3,                                  "enum 3"),
  CMD_OPTION_ENUM         ("e4",       '4',0,0,enumValue,  ENUM1,ENUM4,                                  "enum 4"),

  CMD_OPTION_SPECIAL      ("special1", 0  ,0,1,specialValue,parseSpecial1,123,0,                         "special1",NULL),
  CMD_OPTION_SPECIAL      ("special2", 's',0,1,specialValue,parseSpecial2,123,1,                         "special2","abc"),
  CMD_OPTION_INTEGER      ("extended", 'i',1,0,extendValue,0,0,123,NULL                                  "extended integer"),

  CMD_OPTION_BOOLEAN      ("help",     'h',0,0,helpFlag,   FALSE,                                        "output this help"),

  CMD_OPTION_DEPRECATED   ("deprecated",0,0,1,deprecatedValue,parseDeprecated,NULL,"new name"),
};

*/

/***************************** Variables ******************************/

/******************************* Macros *******************************/

/***********************************************************************\
* Name   : CMD_VALUE_UNIT_ARRAY
* Purpose: define unit array
* Input  : ... - unit values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_VALUE_UNIT_ARRAY(...) \
{ \
  __VA_ARGS__ \
  {NULL,0} \
}; \

/***********************************************************************\
* Name   : CMD_VALUE_SELECT_ARRAY
* Purpose: define select array
* Input  : ... - select values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_VALUE_SELECT_ARRAY(...) \
{ \
  __VA_ARGS__ \
  {NULL,0,NULL} \
}

/***********************************************************************\
* Name   : CMD_VALUE_SET_ARRAY
* Purpose: define set array
* Input  : ... - set values
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_VALUE_SET_ARRAY(...) \
{ \
  __VA_ARGS__ \
  {NULL,0,NULL} \
}

/***********************************************************************\
* Name   : CMD_OPTION_INTEGER, CMD_OPTION_INTEGER_RANGE
* Purpose: define an int command line option
* Input  : name                - option name
*          shortName           - option short name or NULL
*          helpLevel           - help level (0..n)
*          priority            - evaluation priority
*          variable            - variable
*          min,max             - min./max. value
*          units               - unit definition array or NULL
*          description         - description
*          descriptionArgument - optional description argument text
* Output : -
* Return : -
* Notes  : help output:
*            - output a default value if default value != 0
*            - output descriptionArgument if not NULL and default value
*              is not in range of MIN_INT..MAX_INT/MIN_INT64..MAX_INT64
\***********************************************************************/

#define CMD_OPTION_INTEGER(name,shortName,helpLevel,priority,variable,isSetVariablePointer,min,max,units,description,descriptionArgument) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_INTEGER,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,min,max,units,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description \
  }
#define CMD_OPTION_INTEGER_RANGE(name,shortName,helpLevel,priority,variable,isSetVariablePointer,min,max,units,description,descriptionArgument) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_INTEGER,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {TRUE,min,max,units,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_INTEGER64, CMD_OPTION_INTEGER64_RANGE
* Purpose: define an int64 command line option
* Input  : name                - option name
*          shortName           - option short name or NULL
*          helpLevel           - help level (0..n)
*          priority            - evaluation priority
*          variable            - variable
*          min,max             - min./max. value
*          units               - unit definition array or NULL
*          description         - description
*          descriptionArgument - optional description argument text
* Output : -
* Return : -
* Notes  : help output:
*            - output a default value if default value != 0
*            - output descriptionArgument if not NULL and default value
*              is not in range of MIN_INT..MAX_INT/MIN_INT64..MAX_INT64
\***********************************************************************/

#define CMD_OPTION_INTEGER64(name,shortName,helpLevel,priority,variable,isSetVariablePointer,min,max,units,description,descriptionArgument) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_INTEGER64,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,min,max,units,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description \
  }
#define CMD_OPTION_INTEGER64_RANGE(name,shortName,helpLevel,priority,variable,isSetVariablePointer,min,max,units,description,descriptionArgument) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_INTEGER64,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {TRUE,min,max,units,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
   }

/***********************************************************************\
* Name   : CMD_OPTION_DOUBLE, CMD_OPTION_DOUBLE_RANGE
* Purpose: define an double command line option
* Input  : name         - option name
*          shortName    - option short name or NULL
*          helpLevel    - help level (0..n)
*          priority     - evaluation priority
*          variable     - variable
*          min,max      - min./max. value
*          units        - unit definition array or NULL
*          description  - description
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_OPTION_DOUBLE(name,shortName,helpLevel,priority,variable,isSetVariablePointer,min,max,units,description) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_DOUBLE,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,min,max,units,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }
#define CMD_OPTION_DOUBLE_RANGE(name,shortName,helpLevel,priority,variable,isSetVariablePointer,min,max,units,description) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_DOUBLE,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {TRUE,min,max,units,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_BOOLEAN, CMD_OPTION_BOOLEAN_YESNO
* Purpose: define an bool command line option
* Input  : name         - option name
*          shortName    - option short name or NULL
*          helpLevel    - help level (0..n)
*          priority     - evaluation priority
*          variable     - variable
*          description  - description
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_OPTION_BOOLEAN(name,shortName,helpLevel,priority,variable,isSetVariablePointer,description) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_BOOLEAN,\
    {&variable},\
    isSetVariablePointer, \
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }
#define CMD_OPTION_BOOLEAN_YESNO(name,shortName,helpLevel,priority,variable,isSetVariablePointer,description) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_BOOLEAN,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {TRUE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_INCREMENT
* Purpose: define an increment command line option
* Input  : name        - option name
*          shortName   - option short name or NULL
*          helpLevel   - help level (0..n)
*          priority    - evaluation priority
*          variable    - variable
*          min,max     - min./max. value
*          description - description
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_OPTION_INCREMENT(name,shortName,helpLevel,priority,variable,isSetVariablePointer,min,max,description) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_INCREMENT,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,min,max,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_ENUM
* Purpose: define an enum command line option
* Input  : name         - option name
*          shortName    - option short name or NULL
*          helpLevel    - help level (0..n)
*          priority     - evaluation priority
*          variable     - variable
*          value        - enum value
*          description  - description
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_OPTION_ENUM(name,shortName,helpLevel,priority,variable,isSetVariablePointer,value,description) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_ENUM,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {value},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_SELECT
* Purpose: define an select command line option
* Input  : name         - option name
*          shortName    - option short name or NULL
*          helpLevel    - help level (0..n)
*          priority     - evaluation priority
*          variable     - variable
*          selects      - select definition array
*          description  - description
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_OPTION_SELECT(name,shortName,helpLevel,priority,variable,isSetVariablePointer,selects,description) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_SELECT,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {selects},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_SET
* Purpose: define an set (multiple values) command line option
* Input  : name         - option name
*          shortName    - option short name or NULL
*          helpLevel    - help level (0..n)
*          priority     - evaluation priority
*          variable     - variable
*          set          - set definition array
*          description  - description
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_OPTION_SET(name,shortName,helpLevel,priority,variable,isSetVariablePointer,set,description) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_SET,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {set},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_CSTRING
* Purpose: define an C string command line option
* Input  : name                - option name
*          shortName           - option short name or NULL
*          helpLevel           - help level (0..n)
*          priority            - evaluation priority
*          variable            - variable
*          description         - description
*          descriptionArgument - optional description argument text
* Output : -
* Return : -
* Notes  : help output:
*            - output descriptionArgument as argument name
\***********************************************************************/

#define CMD_OPTION_CSTRING(name,shortName,helpLevel,priority,variable,isSetVariablePointer,description,descriptionArgument) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_CSTRING,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {descriptionArgument},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_CSTRING
* Purpose: define an string command line option
* Input  : name                - option name
*          shortName           - option short name or NULL
*          helpLevel           - help level (0..n)
*          priority            - evaluation priority
*          variable            - variable
*          description         - description
*          descriptionArgument - optional description argument text
* Output : -
* Return : -
* Notes  : help output:
*            - output descriptionArgument as argument name
\***********************************************************************/

#define CMD_OPTION_STRING(name,shortName,helpLevel,priority,variable,isSetVariablePointer,description,descriptionArgument) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_STRING,\
    {&variable},\
    isSetVariablePointer,\
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {descriptionArgument},\
    {NULL,NULL,0,NULL},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_SPECIAL
* Purpose: define an special command line option
* Input  : name                - option name
*          shortName           - option short name or NULL
*          helpLevel           - help level (0..n)
*          priority            - evaluation priority
*          variable            - variable
*          parseSpecial        - parse function
*          userData            - user data for parse function
*          argumentCount       - number of arguments for option
*          description         - description
*          descriptionArgument - optional description argument text
* Output : -
* Return : -
* Notes  : help output:
*            - output descriptionArgument as argument name
\***********************************************************************/

#define CMD_OPTION_SPECIAL(name,shortName,helpLevel,priority,variablePointer,isSetVariablePointer,parseSpecial,userData,argumentCount,description,descriptionArgument) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_SPECIAL,\
    {variablePointer},\
    isSetVariablePointer, \
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {parseSpecial,userData,argumentCount,descriptionArgument},\
    {NULL,NULL,0,NULL},\
    description\
  }

/***********************************************************************\
* Name   : CMD_OPTION_DEPRECATED
* Purpose: define an deprecated command line option
* Input  : name                - option name
*          shortName           - option short name or NULL
*          helpLevel           - help level (0..n)
*          priority            - evaluation priority
*          variable            - variable
*          parseDeprecated     - parse function
*          userData            - user data for parse function
*          argumentCount       - number of arguments for option
*          newOptionName       - new option name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define CMD_OPTION_DEPRECATED(name,shortName,helpLevel,priority,variablePointer,isSetVariablePointer,parseDeprecated,userData,argumentCount,newOptionName) \
  {\
    name,\
    shortName,\
    helpLevel,\
    priority,\
    CMD_OPTION_TYPE_DEPRECATED,\
    {variablePointer},\
    isSetVariablePointer, \
    {0,0LL,0.0,FALSE,0,{0},{NULL}},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0,0,NULL,NULL},\
    {FALSE,0.0,0.0,NULL,NULL},\
    {FALSE},\
    {FALSE,0,0,NULL},\
    {0},\
    {NULL},\
    {NULL},\
    {NULL},\
    {NULL,NULL,0,NULL},\
    {parseDeprecated,userData,argumentCount,newOptionName},\
    NULL\
  }

#ifndef NDEBUG
  #define CmdOption_init(...) __CmdOption_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define CmdOption_done(...) __CmdOption_done(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************
* Name   : CmdOption_init
* Purpose: init command line options with default values (read from
*          variables)
* Input  : commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
* Output : -
* Return : TRUE if command line parsed, FALSE on error
* Notes  :
***********************************************************************/

#ifdef NDEBUG
  bool CmdOption_init(CommandLineOption commandLineOptions[],
                      uint              commandLineOptionCount
                     );
#else /* not NDEBUG */
  bool __CmdOption_init(const char        *__fileName__,
                        ulong             __lineNb__,
                        CommandLineOption commandLineOptions[],
                        uint              commandLineOptionCount
                       );
#endif /* NDEBUG */

/***********************************************************************
* Name   : CmdOption_done
* Purpose: deinit command line options
* Input  : commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
* Output : -
* Return : -
* Notes  :
***********************************************************************/

#ifdef NDEBUG
  void CmdOption_done(CommandLineOption commandLineOptions[],
                      uint              commandLineOptionCount
                     );
#else /* not NDEBUG */
  void __CmdOption_done(const char        *__fileName__,
                        ulong             __lineNb__,
                        CommandLineOption commandLineOptions[],
                        uint              commandLineOptionCount
                       );
#endif /* NDEBUG */

/***********************************************************************
* Name   : CmdOption_parse
* Purpose: parse command line options
* Input  : argv                    - command line arguments
*          argc                    - number of command line arguments
*          commandLineOptions      - array with command line options
*                                    spezification
*          commandLineOptionCount  - size of command line options array
*          minPriority,maxPriority - min./max. command line option
*                                    priority or
*          outputHandle            - error/warning output handle or NULL
*          commandPrioritySet      - priority setCMD_PRIORITY_ANY
*          errorPrefix             - error prefix or NULL
*          warningPrefix           - warning prefix or NULL
* Output : arguments
*          argumentsCount
* Return : TRUE if command line parsed, FALSE on error
* Notes  :
***********************************************************************/

bool CmdOption_parse(const char              *argv[],
                     int                     *argc,
                     const CommandLineOption commandLineOptions[],
                     uint                    commandLineOptionCount,
                     uint                    minPriority,
                     uint                    maxPriority,
                     FILE                    *outputHandle,
                     const char              *errorPrefix,
                     const char              *warningPrefix
                    );

/***********************************************************************
* Name   : CmdOption_find
* Purpose: find command line option
* Input  : name                   - command line option name
*          commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
* Output : -
* Return : command line option or NULL if not found
* Notes  :
***********************************************************************/

const CommandLineOption *CmdOption_find(const char              *name,
                                        const CommandLineOption commandLineOptions[],
                                        uint                    commandLineOptionCount
                                       );

/***********************************************************************
* Name   : CmdOption_parseString
* Purpose: parse command line from string
* Input  : commandLineOption - command line option
*          value             - value
* Output : -
* Return : TRUE if command line option parsed, FALSE on error
* Notes  :
***********************************************************************/

bool CmdOption_parseString(const CommandLineOption *commandLineOption,
                           const char              *value
                          );

/***********************************************************************\
* Name   : CmdOption_getIntegerOption
* Purpose: get integer option  value
* Input  : value             - value variable
*          string            - string
*          option            - option name
*          units             - units array or NULL
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

bool CmdOption_getIntegerOption(int                   *value,
                                const char            *string,
                                const char            *option,
                                const CommandLineUnit *units
                               );

/***********************************************************************\
* Name   : CmdOption_getInteger64Option
* Purpose: get integer option value
* Input  : value             - value variable
*          string            - string
*          option            - option name
*          units             - units array or NULL
* Output : value - value
* Return : TRUE if got integer, false otherwise
* Notes  : -
\***********************************************************************/

bool CmdOption_getInteger64Option(int64                 *value,
                                  const char            *string,
                                  const char            *option,
                                  const CommandLineUnit *units
                                 );

/***********************************************************************\
* Name   : CmdOption_selectToString
* Purpose: get select string
* Input  : selects       - select name/value array
*          value         - value
*          defaultString - default string or NULL
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

const char *CmdOption_selectToString(const CommandLineOptionSelect selects[],
                                     uint                          value,
                                     const char                    *defaultString
                                    );

/***********************************************************************
* Name   : CmdOption_printHelp
* Purpose: print command line options help
* Input  : outputHandle           - file handle to print to
*          commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
*          helpLevel              - help level or CMD_HELP_LEVEL_ALL
* Output :
* Return :
* Notes  :
***********************************************************************/

void CmdOption_printHelp(FILE                    *outputHandle,
                         const CommandLineOption commandLineOptions[],
                         uint                    commandLineOptionCount,
                         int                     helpLevel
                        );

#ifdef __GNUG__
}
#endif

#endif /* __CMD_OPTIONS__ */

/* end of file */
