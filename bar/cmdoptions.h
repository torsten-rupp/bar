/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/cmdoptions.h,v $
* $Revision: 1.11 $
* $Author: torsten $
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

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/

#define CMD_LEVEL_ALL    -1
#define CMD_PRIORITY_ANY -1

/***************************** Datatypes ******************************/
typedef enum
{
  CMD_OPTION_TYPE_INTEGER,
  CMD_OPTION_TYPE_INTEGER64,
  CMD_OPTION_TYPE_DOUBLE,
  CMD_OPTION_TYPE_BOOLEAN,
  CMD_OPTION_TYPE_ENUM,
  CMD_OPTION_TYPE_SELECT,
  CMD_OPTION_TYPE_SET,
  CMD_OPTION_TYPE_STRING,
  CMD_OPTION_TYPE_SPECIAL
} CommandLineOptionTypes;

typedef struct
{
  const char *name;
  uint64     factor;
} CommandLineUnit;

typedef struct
{
  const char *name;
  int        value;
  const char *description;
} CommandLineOptionSelect;

typedef struct
{
  const char *name;
  int        value;
  const char *description;
} CommandLineOptionSet;

typedef struct CommandLineOption
{
  const char             *name;
  char                   shortName;
  unsigned int           level;
  unsigned int           priority;
  CommandLineOptionTypes type;
  union
  {
    void   *pointer;
    int    *i;
    int64  *l;
    double *d;
    bool   *b;
    uint   *enumeration;
    uint   *select;
    ulong  *set;
    char   **string;
    void   *special;
  } variable;
  struct
  {
    int    i;
    int64  l;
    double d;
    bool   b;
    union
    {
      uint  enumeration;
      uint  select;
      ulong set;
    };
    union
    {
      const char *string;
      const void *p;
    };
  } defaultValue;
  struct
  {
    bool                          rangeFlag;            // TRUE iff range should be printed in help
    int                           min,max;              // valid range
    const CommandLineUnit         *units;               // list with units
    uint                          unitCount;            // number of units
  } integerOption;
  struct
  {
    bool                          rangeFlag;            // TRUE iff range should be printed in help
    int64                         min,max;              // valid range
    const CommandLineUnit         *units;               // list with units
    uint                          unitCount;            // number of units
  } integer64Option;
  struct
  {
    bool                          rangeFlag;            // TRUE iff range should be printed in help
    double                        min,max;              // valid range
    const CommandLineUnit         *units;               // list with units
    uint                          unitCount;            // number of units
  } doubleOption;
  struct
  {
    bool                          yesnoFlag;            // TRUE iff yes/no should be printed in help
  } booleanOption;
  struct
  {
    uint                          enumerationValue;     // emumeration value for this enumeration
  } enumOption;
  struct
  {
    const CommandLineOptionSelect *selects;             // list with select values
    uint                          selectCount;          // number of select values
  } selectOption;
  struct
  {
    const CommandLineOptionSet    *set;                 // list with set values
    uint                          setCount;             // number of set values
  } setOption;
  struct
  {
    const char                    *helpArgument;        // text for help argument
  } stringOption;
  struct
  {
    bool(*parseSpecial)(void *userData, void *variable, const char *name, const char *value, const void *defaultValue);
    void                          *userData;            // user data for parse special
    const char                    *helpArgument;        // text for help argument
  } specialOption;
  const char                    *description;
} CommandLineOption;

typedef union
{
  int    n;
  int64  l;
  double d;
  bool   b;
  uint   enumeration;
  uint   select;
  char   *string;
  void   *special;
} OptionVariable;

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
  CMD_OPTION_INTEGER      ("integer", 'i',0,0,intValue,   0,0,123,NULL                                  "integer value"),
  CMD_OPTION_INTEGER      ("unit",    'u',0,0,intValue,   0,0,123,COMMAND_LINE_UNITS                    "integer value with unit"),
  CMD_OPTION_INTEGER_RANGE("range1",  'r',0,0,intValue,   0,0,123,COMMAND_LINE_UNITS                    "integer value with unit and range"),

  CMD_OPTION_DOUBLE       ("double",  'd',0,0,doubleValue,0.0,-2.0,4.0,                                 "double value"),
  CMD_OPTION_DOUBLE_RANGE ("range2",   0,  0,0,doubleValue,0.0,-2.0,4.0,                                 "double value with range"),

  CMD_OPTION_BOOLEAN_YESNO("bool",    'b',0,0,boolValue,  FALSE,                                        "bool value 1"),

  CMD_OPTION_SELECT       ("type",    't',0,0,outputType, 1,      COMMAND_LINE_OPTIONS_SELECT_TYPES,    "select value",NULL),
  CMD_OPTION_SET          ("set",     0,  0,0,setType,    0,      COMMAND_LINE_OPTIONS_SET_TYPES,       "set value",NULL),

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
#define CMD_OPTION_INTEGER(name,shortName,level,priority,variable,defaultValue,min,max,units,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_INTEGER,\
    {&variable},\
    {defaultValue,0LL,0.0,FALSE,{0},{NULL}},\
    {FALSE,min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description \
  }
#define CMD_OPTION_INTEGER_RANGE(name,shortName,level,priority,variable,defaultValue,min,max,units,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_INTEGER,\
    {&variable},\
    {defaultValue,0LL,0.0,FALSE,{0},{NULL}},\
    {TRUE,min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
  }

#define CMD_OPTION_INTEGER64(name,shortName,level,priority,variable,defaultValue,min,max,units,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_INTEGER64,\
    {&variable},\
    {0,defaultValue,0.0,FALSE,{0},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description \
  }
#define CMD_OPTION_INTEGER64_RANGE(name,shortName,level,priority,variable,defaultValue,min,max,units,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_INTEGER64,\
    {&variable},\
    {0,defaultValue,0.0,FALSE,{0},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {TRUE,min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
   }

#define CMD_OPTION_DOUBLE(name,shortName,level,priority,variable,defaultValue,min,max,units,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_DOUBLE,\
    {&variable},\
    {0,0LL,defaultValue,FALSE,{0},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {FALSE,min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
  }
#define CMD_OPTION_DOUBLE_RANGE(name,shortName,level,priority,variable,defaultValue,min,max,units,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_DOUBLE,\
    {&variable},\
    {0,0LL,defaultValue,FALSE,{0},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {TRUE,min,max,units,sizeof(units)/sizeof(CommandLineUnit)},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
  }

#define CMD_OPTION_BOOLEAN(name,shortName,level,priority,variable,defaultValue,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_BOOLEAN,\
    {&variable},\
    {0,0LL,0.0,defaultValue,{0},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
  }
#define CMD_OPTION_BOOLEAN_YESNO(name,shortName,level,priority,variable,defaultValue,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_BOOLEAN,\
    {&variable},\
    {0,0LL,0.0,defaultValue,{0},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {TRUE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
  }

#define CMD_OPTION_ENUM(name,shortName,level,priority,variable,defaultValue,value,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_ENUM,\
    {&variable},\
    {0,0LL,0.0,FALSE,{defaultValue},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {value},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
  }

#define CMD_OPTION_SELECT(name,shortName,level,priority,variable,defaultValue,selects,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_SELECT,\
    {&variable},\
    {0,0LL,0.0,FALSE,{defaultValue},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {selects,sizeof(selects)/sizeof(CommandLineOptionSelect)},\
    {NULL,0},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
  }

#define CMD_OPTION_SET(name,shortName,level,priority,variable,defaultValue,set,description) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_SET,\
    {&variable},\
    {0,0LL,0.0,FALSE,{defaultValue},{NULL}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {set,sizeof(set)/sizeof(CommandLineOptionSet)},\
    {NULL},\
    {NULL,NULL,NULL},\
    description\
  }

#define CMD_OPTION_STRING(name,shortName,level,priority,variable,defaultValue,description,helpArgument) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_STRING,\
    {&variable},\
    {0,0LL,0.0,FALSE,{0},{defaultValue}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {helpArgument},\
    {NULL,NULL,NULL},\
    description\
  }

#define CMD_OPTION_SPECIAL(name,shortName,level,priority,variablePointer,defaultValue,parseSpecial,userData,description,helpArgument) \
  {\
    name,\
    shortName,\
    level,\
    priority,\
    CMD_OPTION_TYPE_SPECIAL,\
    {variablePointer},\
    {0,0LL,0.0,FALSE,{0},{(void*)defaultValue}},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0,0,NULL,0},\
    {FALSE,0.0,0.0,NULL,0},\
    {FALSE},\
    {0},\
    {NULL,0},\
    {NULL,0},\
    {NULL},\
    {parseSpecial,userData,helpArgument},\
    description\
  }

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************
* Name   : CmdOption_init
* Purpose: init command line options with default values
* Input  : commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
* Output : -
* Return : TRUE if command line parsed, FALSE on error
* Notes  :
***********************************************************************/

bool CmdOption_init(const CommandLineOption commandLineOptions[],
                    uint                    commandLineOptionCount
                   );

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
void CmdOption_done(const CommandLineOption commandLineOptions[],
                    uint                    commandLineOptionCount
                   );

/***********************************************************************
* Name   : CmdOption_parse
* Purpose: parse command line options
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

bool CmdOption_parse(const char              *argv[],
                     int                     *argc,
                     const CommandLineOption commandLineOptions[],
                     uint                    commandLineOptionCount,
                     int                     commandPriority,
                     FILE                    *errorOutputHandle,
                     const char              *errorPrefix
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

/***********************************************************************
* Name   : CmdOption_printHelp
* Purpose: print command line options help
* Input  : outputHandle           - file handle to print to
*          commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
*          level                  - help level or CMD_LEVEL_ALL
* Output :
* Return :
* Notes  :
***********************************************************************/

void CmdOption_printHelp(FILE                    *outputHandle,
                         const CommandLineOption commandLineOptions[],
                         uint                    commandLineOptionCount,
                         int                     level
                        );

#ifdef __GNUG__
}
#endif

#endif /* __CMD_OPTIONS__ */

/* end of file */
