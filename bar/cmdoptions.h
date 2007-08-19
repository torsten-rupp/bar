/**********************************************************************
*
* $Source: /home/torsten/cvs/bar/cmdoptions.h,v $
* $Revision: 1.2 $
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
#include <assert.h>

#include "global.h"

/********************** Conditional compilation ***********************/

/**************************** Constants *******************************/

/***************************** Datatypes ******************************/
typedef enum
{
  CMD_OPTION_TYPE_INTEGER,
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
    long       *n;
    double     *d;
    bool       *b;
    int        *enumeration;
    int        *select;
    const char **string;
    void       *special;
  } variable;
  struct
  {
    long       n;
    double     d;
    bool       b;
    union
    {
      int enumeration;
      int select;
    };
    union
    {
      const char *string;
      const void *p;
    };
  } defaultValue;
  int                           enumerationValue;
  const CommandLineOptionSelect *selects;
  int                           selectCount;
  bool(*parseSpecial)(void *variable, const char *value, const void *defaultValue, void *userData);
  void                          *userData;
  const char                    *description;
} CommandLineOption;

/* example

CMD_OPTION_INTEGER(<long name>,<short name>,<priority>,<variable>,<default value>,<help text>)
CMD_OPTION_DOUBLE (<long name>,<short name>,<priority>,<variable>,<default value>,<help text>)
CMD_OPTION_BOOLEAN(<long name>,<short name>,<priority>,<variable>,<default value>,<help text>)
CMD_OPTION_ENUM   (<long name>,<short name>,<priority>,<variable>,<default value>,<value>,<help text>)
CMD_OPTION_STRING (<long name>,<short name>,<priority>,<variable>,<default value>,<help text>)
CMD_OPTION_SPECIAL(<long name>,<short name>,<priority>,<function>,<default value>,<help text>)

const CommandLineOptionSelect COMMAND_LINE_OPTIONS_SELECT_OUTPUTYPE[] =
{
  {"c",   1,"type1"},
  {"h",   2,"type2"},
  {"both",3,"type3"},
};

const CommandLineOption COMMAND_LINE_OPTIONS[] =
{
  CMD_OPTION_INTEGER("integer",'i',0,intValue,   0,                                            "integer value"),

  CMD_OPTION_DOUBLE ("double", 'd',0,doubleValue,0.0,                                          "double value"),

  CMD_OPTION_BOOLEAN("bool",   'b',0,boolValue,  FALSE,                                        "bool value 1"),

  CMD_OPTION_SELECT ("type",   't',0,outputType, 1,      COMMAND_LINE_OPTIONS_SELECT_OUTPUTYPE,"select value"),

  CMD_OPTION_STRING ("string", 0,  0,stringValue,"",                                           "string value"),

  CMD_OPTION_ENUM   ("e1",     '1',0,enumValue,  0,ENUM1,                                      "enum 1"), 
  CMD_OPTION_ENUM   ("e2",     '2',0,enumValue,  0,ENUM2,                                      "enum 2"), 
  CMD_OPTION_ENUM   ("e3",     '3',0,enumValue,  0,ENUM3,                                      "enum 3"), 
  CMD_OPTION_ENUM   ("e4",     '4',0,enumValue,  0,ENUM4,                                      "enum 4"), 

  CMD_OPTION_SPECIAL("special",'s',1,specialValue,parseSpecial,123,                            "special"), 

  CMD_OPTION_BOOLEAN("help",   'h',0,helpFlag,   FALSE,                                        "output this help"),
};

*/

/***************************** Variables ******************************/

/******************************* Macros *******************************/
#define CMD_OPTION_INTEGER(name,shortName,priority,variable,defaultValue,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_INTEGER,{&variable},{defaultValue,0.0,FALSE,{0},{NULL}},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_DOUBLE(name,shortName,priority,variable,defaultValue,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_DOUBLE,{&variable},{0,defaultValue,FALSE,{0},{NULL}},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_BOOLEAN(name,shortName,priority,variable,defaultValue,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_BOOLEAN,{&variable},{0,0.0,defaultValue,{0},{NULL}},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_ENUM(name,shortName,priority,variable,defaultValue,value,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_ENUM,{&variable},{0,0.0,FALSE,{defaultValue},{NULL}},value,NULL,0,NULL,NULL,description }

#define CMD_OPTION_SELECT(name,shortName,priority,variable,defaultValue,selects,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_SELECT,{&variable},{0,0.0,FALSE,{defaultValue},{NULL}},0,selects,sizeof(selects)/sizeof(selects[0]),NULL,NULL,description }

#define CMD_OPTION_STRING(name,shortName,priority,variable,defaultValue,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_STRING,{&variable},{0,0.0,FALSE,{0},{defaultValue}},0,NULL,0,NULL,NULL,description }

#define CMD_OPTION_SPECIAL(name,shortName,priority,variable,defaultValue,parseSpecial,userData,description) \
  { name,shortName,priority,CMD_OPTION_TYPE_SPECIAL,{&variable},{0,0.0,FALSE,{0},{(void*)defaultValue}},0,NULL,0,parseSpecial,userData,description }

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
