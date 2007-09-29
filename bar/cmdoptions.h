/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/cmdoptions.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: command line options parser
* Systems :
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

/***************************** Datatypes ******************************/
typedef enum
{
  CMD_OPTION_TYPE_INTEGER,
  CMD_OPTION_TYPE_INTEGER64,
  CMD_OPTION_TYPE_DOUBLE,
  CMD_OPTION_TYPE_BOOLEAN,
  CMD_OPTION_TYPE_ENUM,
  CMD_OPTION_TYPE_SELECT,
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

typedef struct CommandLineOption
{
  const char             *name;
  char                   shortName;
  unsigned int           priority;
  CommandLineOptionTypes type;
  union
  {
    void       *pointer;
    int        *n;
    int64      *l;
    double     *d;
    bool       *b;
    uint       *enumeration;
    uint       *select;
    const char **string;
    void       *special;
  } variable;
  struct
  {
    int    n;
    int64  l;
    double d;
    bool   b;
    union
    {
      uint enumeration;
      uint select;
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
  } doubleOption;
  uint                          enumerationValue;       // emumeration value for this enumeration
  const CommandLineOptionSelect *selects;               // list with select values
  uint                          selectCount;            // number of select values
  bool(*parseSpecial)(void *variable, const char *value, const void *defaultValue, void *userData);
  void                          *userData;              // user data for parse special
  const char                    *description;
} CommandLineOption;

/* example

CMD_OPTION_INTEGER        (<long name>,<short name>,<priority>,<variable>,<default value>,<min>,<max>,<units>,<help text>)
CMD_OPTION_INTEGER_RANGE  (<long name>,<short name>,<priority>,<variable>,<default value>,<min>,<max>,<units>,<help text>)
CMD_OPTION_INTEGER64      (<long name>,<short name>,<priority>,<variable>,<default value>,<min>,<max>,<units>,<help text>)
CMD_OPTION_INTEGER64_RANGE(<long name>,<short name>,<priority>,<variable>,<default value>,<min>,<max>,<units>,<help text>)
CMD_OPTION_DOUBLE         (<long name>,<short name>,<priority>,<variable>,<default value>,                    <help text>)
CMD_OPTION_DOUBLE_RANGE   (<long name>,<short name>,<priority>,<variable>,<default value>,<min>,<max>,        <help text>)
CMD_OPTION_BOOLEAN        (<long name>,<short name>,<priority>,<variable>,<default value>,                    <help text>)
CMD_OPTION_ENUM           (<long name>,<short name>,<priority>,<variable>,<default value>,<value>,            <help text>)
CMD_OPTION_STRING         (<long name>,<short name>,<priority>,<variable>,<default value>,                    <help text>)
CMD_OPTION_SPECIAL        (<long name>,<short name>,<priority>,<function>,<default value>,                    <help text>)

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
  CMD_OPTION_INTEGER      ("integer",'i',0,intValue,   0,0,123,NULL                                  "integer value"),
  CMD_OPTION_INTEGER      ("unit",   'u',0,intValue,   0,0,123,COMMAND_LINE_UNITS                    "integer value with unit"),
  CMD_OPTION_INTEGER_RANGE("range1", 'r',0,intValue,   0,0,123,COMMAND_LINE_UNITS                    "integer value with unit and range"),

  CMD_OPTION_DOUBLE       ("double", 'd',0,doubleValue,0.0,-2.0,4.0,                                 "double value"),
  CMD_OPTION_DOUBLE_RANGE ("range2", 0,  0,doubleValue,0.0,-2.0,4.0,                                 "double value with range"),

  CMD_OPTION_BOOLEAN      ("bool",   'b',0,boolValue,  FALSE,                                        "bool value 1"),

  CMD_OPTION_SELECT       ("type",   't',0,outputType, 1,      COMMAND_LINE_OPTIONS_SELECT_OUTPUTYPE,"select value"),

  CMD_OPTION_STRING       ("string", 0,  0,stringValue,"",                                           "string value"),

  CMD_OPTION_ENUM         ("e1",     '1',0,enumValue,  0,ENUM1,                                      "enum 1"), 
  CMD_OPTION_ENUM         ("e2",     '2',0,enumValue,  0,ENUM2,                                      "enum 2"), 
  CMD_OPTION_ENUM         ("e3",     '3',0,enumValue,  0,ENUM3,                                      "enum 3"), 
  CMD_OPTION_ENUM         ("e4",     '4',0,enumValue,  0,ENUM4,                                      "enum 4"), 

  CMD_OPTION_SPECIAL      ("special",'s',1,specialValue,parseSpecial,123,                            "special"), 

  CMD_OPTION_BOOLEAN      ("help",   'h',0,helpFlag,   FALSE,                                        "output this help"),
};

*/

/***************************** Variables ******************************/

/******************************* Macros *******************************/
#define CMD_OPTION_INTEGER(name,shortName,priority,variable,defaultValue,min,max,units,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_INTEGER,{&variable},{defaultValue,0LL,0.0,FALSE,{0},{NULL}},{FALSE,min,max,units,sizeof(units)/sizeof(units[0])},{FALSE,0,0,NULL,0},{0.0,0.0},0,NULL,0,NULL,NULL,description }
#define CMD_OPTION_INTEGER_RANGE(name,shortName,priority,variable,defaultValue,min,max,units,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_INTEGER,{&variable},{defaultValue,0LL,0.0,FALSE,{0},{NULL}},{TRUE,min,max,units,sizeof(units)/sizeof(units[0])},{FALSE,0,0,NULL,0},{0.0,0.0},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_INTEGER64(name,shortName,priority,variable,defaultValue,min,max,units,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_INTEGER64,{&variable},{0,defaultValue,0.0,FALSE,{0},{NULL}},{FALSE,0,0,NULL,0},{FALSE,min,max,units,sizeof(units)/sizeof(units[0])},{0.0,0.0},0,NULL,0,NULL,NULL,description }
#define CMD_OPTION_INTEGER64_RANGE(name,shortName,priority,variable,defaultValue,min,max,units,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_INTEGER64,{&variable},{0,defaultValue,0.0,FALSE,{0},{NULL}},{FALSE,0,0,NULL,0},{TRUE,min,max,units,sizeof(units)/sizeof(units[0])},{0.0,0.0},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_DOUBLE(name,shortName,priority,variable,defaultValue,min,max,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_DOUBLE,{&variable},{0,0LL,defaultValue,FALSE,{0},{NULL}},{FALSE,0,0,NULL,0},{FALSE,0,0,NULL,0},{FALSE,min,max},0,NULL,0,NULL,NULL,description }
#define CMD_OPTION_DOUBLE_RANGE(name,shortName,priority,variable,defaultValue,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_DOUBLE,{&variable},{0,0LL,defaultValue,FALSE,{0},{NULL}},{FALSE,0,0,NULL,0},{FALSE,0,0,NULL,0},{TRUE,min,max},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_BOOLEAN(name,shortName,priority,variable,defaultValue,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_BOOLEAN,{&variable},{0,0LL,0.0,defaultValue,{0},{NULL}},{FALSE,0,0,NULL,0},{FALSE,0,0,NULL,0},{FALSE,0.0,0.0},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_ENUM(name,shortName,priority,variable,defaultValue,value,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_ENUM,{&variable},{0,0LL,0.0,FALSE,{defaultValue},{NULL}},{FALSE,0,0,NULL,0},{FALSE,0,0,NULL,0},{FALSE,0.0,0.0},value,NULL,0,NULL,NULL,description }

#define CMD_OPTION_SELECT(name,shortName,priority,variable,defaultValue,selects,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_SELECT,{&variable},{0,0LL,0.0,FALSE,{defaultValue},{NULL}},{FALSE,0,0,NULL,0},{FALSE,0,0,NULL,0},{FALSE,0.0,0.0},0,selects,sizeof(selects)/sizeof(selects[0]),NULL,NULL,description }

#define CMD_OPTION_STRING(name,shortName,priority,variable,defaultValue,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_STRING,{&variable},{0,0LL,0.0,FALSE,{0},{defaultValue}},{FALSE,0,0,NULL,0},{FALSE,0,0,NULL,0},{FALSE,0.0,0.0},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_SPECIAL(name,shortName,priority,variable,defaultValue,parseSpecial,userData,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_SPECIAL,{&variable},{0,0LL,0.0,FALSE,{0},{(void*)defaultValue}},{FALSE,0,0,NULL,0},{FALSE,0,0,NULL,0},{FALSE,0.0,0.0},0,NULL,0,parseSpecial,userData,description }

/***************************** Functions ******************************/

#ifdef __GNUG__
extern "C" {
#endif

/***********************************************************************
* Name   : cmdOptions_parse
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

bool cmdOptions_parse(const char              *argv[],
                      int                     *argc,
                      const CommandLineOption commandLineOptions[],
                      uint                    commandLineOptionCount,
                      FILE                    *errorOutputHandle,
                      const char              *errorPrefix
                     );

/***********************************************************************
* Name   : cmdOptions_printHelp
* Purpose: print command line options help
* Input  : outputHandle           - file handle to print to
*          commandLineOptions     - array with command line options
*                                   spezification
*          commandLineOptionCount - size of command line options array
* Output :
* Return :
* Notes  :
***********************************************************************/

void cmdOptions_printHelp(FILE                    *outputHandle,
                          const CommandLineOption commandLineOptions[],
                          uint                    commandLineOptionCount
                         );

#ifdef __GNUG__
}
#endif

#endif /* __CMD_OPTIONS__ */

/* end of file */
