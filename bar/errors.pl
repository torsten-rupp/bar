#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/errors.pl,v $
# $Revision$
# $Author$
# Contents: create header/c file definition from errors definition
# Systems: all
#
# ----------------------------------------------------------------------------

# syntax of error definition file
#
# INCLUDE "<file>"
# INCLUDE <<file>>
# ERROR <name> "<text>"
# ERROR <name>
#   <code>
#
# DEFAULT "<text>"
# NONE "<text>"

# ---------------------------- additional packages ---------------------------
use English;
use POSIX;
use Getopt::Std;
use Getopt::Long;

# ---------------------------- constants/variables ---------------------------

# error code:
#
#  b31...b16  b15...b10  b9..b0
#
#    errno   text index  code
#

my $ERROR_CODE_MASK           = "0x000003FF";
my $ERROR_CODE_SHIFT          = 0;
my $ERROR_DATAINDEX_MASK      = "0x0000FC00";
my $ERROR_DATAINDEX_SHIFT     = 10;
my $ERROR_ERRNO_MASK          = "0xFFFF0000";
my $ERROR_ERRNO_SHIFT         = 16;

my $ERROR_MAX_TEXT_LENGTH     = 2048;
my $ERROR_DATAINDEX_MAX_COUNT = 63;

my $PREFIX                    = "ERROR_";

my $cFileName,$hFileName,$javaFileName;
my $errorNumber=0;

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

#***********************************************************************
# Name   : writeCFile
# Purpose: write C file
# Input  : string
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeCFile($)
{
  my $s=shift(@_);

  if ($cFileName ne "")
  {
    print CFILE_HANDLE $s;
  }
}

#***********************************************************************
# Name   : writeHFile
# Purpose: write header file
# Input  : string
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeHFile($)
{
  my $s=shift(@_);

  if ($hFileName ne "")
  {
    print HFILE_HANDLE $s;
  }
}

#***********************************************************************
# Name   : writeJavaFile
# Purpose: write Java file
# Input  : string
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeJavaFile($)
{
  my $s=shift(@_);

  if ($javaFileName ne "")
  {
    print JAVAFILE_HANDLE $s;
  }
}

#***********************************************************************
# Name   : writeCPrefix
# Purpose: write C file prefix
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeCPrefix()
{
  print CFILE_HANDLE "\
#define ERROR_GET_CODE(error)        (((error) & $ERROR_CODE_MASK) >> $ERROR_CODE_SHIFT)
#define ERROR_GET_CODE_TEXT(error)   Error_getCodeText(error)
#define ERROR_GET_TEXTINDEX(error)   (((error) & $ERROR_DATAINDEX_MASK) >> $ERROR_DATAINDEX_SHIFT)
#ifndef NDEBUG
#define ERROR_GET_FILENAME(error)    ((ERROR_GET_TEXTINDEX(error) > 0) ? errorData[ERROR_GET_TEXTINDEX(error)-1].fileName : NONE)
#define ERROR_GET_LINENB(error)      ((ERROR_GET_TEXTINDEX(error) > 0) ? errorData[ERROR_GET_TEXTINDEX(error)-1].lineNb : 0)
#define ERROR_GET_LINENB_TEXT(error) Error_getLineNbText(error)
#else
#define ERROR_GET_FILENAME(error)    NONE
#define ERROR_GET_LINENB(error)      0
#define ERROR_GET_LINENB_TEXT(error) NONE
#endif
#define ERROR_GET_TEXT(error)        ((ERROR_GET_TEXTINDEX(error) > 0) ? errorData[ERROR_GET_TEXTINDEX(error)-1].text : NONE)
#define ERROR_GET_ERRNO(error)       ((int)((error) & $ERROR_ERRNO_MASK) >> $ERROR_ERRNO_SHIFT)
#define ERROR_GET_ERRNO_TEXT(error)  Error_getErrnoText(error)

#define ERROR_CODE        ERROR_GET_CODE(error)
#define ERROR_FILENAME    ERROR_GET_FILENAME(error)
#define ERROR_LINENB      ERROR_GET_LINENB(error)
#define ERROR_LINENB_TEXT ERROR_GET_LINENB_TEXT(error)
#define ERROR_TEXT        ERROR_GET_TEXT(error)
#define ERROR_ERRNO       ERROR_GET_ERRNO(error)
#define ERROR_ERRNO_TEXT  ERROR_GET_ERRNO_TEXT(error)

unsigned int Error_getCode(Errors error)
{
  return ERROR_GET_CODE(error);
}

const char *Error_getCodeText(Errors error)
{
  static char codeText[$ERROR_MAX_TEXT_LENGTH];

  snprintf(codeText,sizeof(codeText)-1,\"0x%03x\",ERROR_GET_CODE(error));
  codeText[sizeof(codeText)-1] = '\\0';

  return codeText;
}

const char *Error_getFileName(Errors error)
{
  #ifndef NDEBUG
    return ERROR_GET_FILENAME(error);
  #else
    UNUSED_VARIABLE(error);

    return NONE;
  #endif
}

const char *Error_getLineNbText(Errors error)
{
  #ifndef NDEBUG
    static char lineNbText[$ERROR_MAX_TEXT_LENGTH];

    snprintf(lineNbText,sizeof(lineNbText)-1,\"%d\",ERROR_GET_LINENB(error));
    lineNbText[sizeof(lineNbText)-1] = '\\0';

    return lineNbText;
  #else
    UNUSED_VARIABLE(error);

    return NULL;
  #endif
}

const char *Error_getLocationText(Errors error)
{
  #ifndef NDEBUG
    static char locationText[$ERROR_MAX_TEXT_LENGTH];

    snprintf(locationText,sizeof(locationText)-1,\"%s, %d\",ERROR_GET_FILENAME(error),ERROR_GET_LINENB(error));
    locationText[sizeof(locationText)-1] = '\\0';

    return locationText;
  #else
    UNUSED_VARIABLE(error);

    return NULL;
  #endif
}

const char *Error_getErrnoText(Errors error)
{
  static char errnoText[$ERROR_MAX_TEXT_LENGTH];

  snprintf(errnoText,sizeof(errnoText)-1,\"%d\",ERROR_GET_ERRNO(error));
  errnoText[sizeof(errnoText)-1] = '\\0';

  return errnoText;
}

const char *Error_getText(Errors error)
{
  static char errorText[$ERROR_MAX_TEXT_LENGTH];

  stringClear(errorText);
  switch (ERROR_GET_CODE(error))
  {
";
}

#***********************************************************************
# Name   : writeCPostfix
# Purpose: write C postfix
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeCPostfix()
{
  if ($defaultText ne "")
  {
    writeCFile("    default: stringSet(errorText,\"$defaultText\",sizeof(errorText)); break;\n");
  }
  print CFILE_HANDLE "\
  }
  if (stringIsEmpty(errorText)) stringSet(errorText,\"unknown\",sizeof(errorText));
  #ifndef NDEBUG
    if (ERROR_FILENAME != NULL)
    {
      stringAppend(errorText,\" at \",sizeof(errorText));
      stringAppend(errorText,ERROR_FILENAME,sizeof(errorText));
      stringAppend(errorText,\", \",sizeof(errorText));
      stringAppend(errorText,ERROR_LINENB_TEXT,sizeof(errorText));
    }
  #endif /* not NDEBUG */

  return errorText;
}
";
}

#***********************************************************************
# Name   : writeHPrefix
# Purpose: write header prefix
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeHPrefix()
{
  print HFILE_HANDLE "\
typedef enum
{
  ".$PREFIX."NONE = 0,
";
}

#***********************************************************************
# Name   : writeHPostfix
# Purpose: write header postfix
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeHPostfix()
{
  print HFILE_HANDLE "\
  ".$PREFIX."UNKNOWN = ".($errorNumber+1)."
} Errors;

#ifdef __cplusplus
  extern \"C\" {
#endif

/***********************************************************************\
* Name   : _Error_dataToIndex
* Purpose: store error data as index
* Input  : fileName - file name
*          lineNb   - line number
*          format   - format string (like printf)
*          ...      - optional arguments for format string
* Output : -
* Return : index
* Notes  : internal usage only!
\***********************************************************************/

#ifndef NDEBUG
int _Error_dataToIndex(const char *fileName, ulong lineNb, const char *format, ...);
#else
int _Error_dataToIndex(const char *format, ...);
#endif

/***********************************************************************\
* Name   : Error_getCode
* Purpose: get error code
* Input  : error - error
* Output : -
* Return : error code
* Notes  : -
\***********************************************************************/

unsigned int Error_getCode(Errors error);

/***********************************************************************\
* Name   : Error_getCodeText
* Purpose: get error code as text (hex)
* Input  : error - error
* Output : -
* Return : text
* Notes  : -
\***********************************************************************/

const char *Error_getCodeText(Errors error);

/***********************************************************************\
* Name   : Error_getFileName
* Purpose: get filename
* Input  : error - error
* Output : -
* Return : filename
* Notes  : -
\***********************************************************************/

const char *Error_getFileName(Errors error);

/***********************************************************************\
* Name   : Error_getLineNbText
* Purpose: get line number text
* Input  : error - error
* Output : -
* Return : line number text
* Notes  : -
\***********************************************************************/

const char *Error_getLineNbText(Errors error);

/***********************************************************************\
* Name   : Error_getLocationText
* Purpose: get location text (filename+line number)
* Input  : error - error
* Output : -
* Return : location text
* Notes  : -
\***********************************************************************/

const char *Error_getLocationText(Errors error);

/***********************************************************************\
* Name   : Error_getErrnoText
* Purpose: get errno text
* Input  : error - error
* Output : -
* Return : errno text
* Notes  : -
\***********************************************************************/

const char *Error_getErrnoText(Errors error);

/***********************************************************************\
* Name   : Error_getText
* Purpose: get error text
* Input  : error - error
* Output : -
* Return : error text
* Notes  : -
\***********************************************************************/

const char *Error_getText(Errors error);

#ifdef __cplusplus
  }
#endif

#endif /* __ERRORS__ */
";
}

#***********************************************************************
# Name   : writeJavaPrefix
# Purpose: write Java prefix
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeJavaPrefix()
{
  print JAVAFILE_HANDLE "class Errors\n";
  print JAVAFILE_HANDLE "{\n";
  print JAVAFILE_HANDLE "  static final int NONE = 0;\n";
}

#***********************************************************************
# Name   : writeJavaPostfix
# Purpose: write Java postfix
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeJavaPostfix()
{
  print JAVAFILE_HANDLE "  static final int UNKNOWN = ".($errorNumber+1).";\n";
  print JAVAFILE_HANDLE "}\n";
}

# ------------------------------ main program  -------------------------------

GetOptions("c=s" => \$cFileName,
           "h=s" => \$hFileName,
           "j=s" => \$javaFileName,
          );

if ($cFileName ne "")
{
  open(CFILE_HANDLE,"> $cFileName");
  print CFILE_HANDLE "#define __ERROR_IMPLEMENTATION__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

#include \"global.h\"

#include \"errors.h\"

// use NONE to avoid warning in strn*-functions which do not accept NULL (this case must be checked before calling strn*
static const char *NONE = NULL;

typedef struct
{
  int  id;
  char text[$ERROR_MAX_TEXT_LENGTH];
  #ifndef NDEBUG
    const char   *fileName;
    unsigned int lineNb;
  #endif /* not NDEBUG */
} ErrorData;

static ErrorData errorData[$ERROR_DATAINDEX_MAX_COUNT];    // last error data
static uint      errorDataCount = 0;                       // last error data count (=max. when all data entries are used; recycle oldest entry if required)
static uint      errorDataId    = 0;                       // total number of error data

#ifndef NDEBUG
int _Error_dataToIndex(const char *fileName, ulong lineNb, const char *format, ...)
#else
int _Error_dataToIndex(const char *format, ...)
#endif
{
  va_list arguments;
  char    text[$ERROR_MAX_TEXT_LENGTH];
  int     index;
  int     minId;
  uint    z,i;

  if (format != NULL)
  {
    // format error text
    va_start(arguments,format);
    vsnprintf(text,sizeof(text),format,arguments);
    va_end(arguments);
  }
  else
  {
    stringClear(text);
  }

  // get new error data id
  errorDataId++;

  // get error data index
  index = -1;
  z = 0;
  while ((z < errorDataCount) && (index == -1))
  {
    if (stringEquals(errorData[z].text,text))
    {
      index = z;
    }
    z++;
  }
  if (index == -1)
  {
    if (errorDataCount < $ERROR_DATAINDEX_MAX_COUNT)
    {
      // use next entry
      index = errorDataCount;
      errorDataCount++;
    }
    else
    {
      // recycle oldest entry (entry with smallest id)
      index = 0;
      minId = INT_MAX;
      for (z = 0; z < $ERROR_DATAINDEX_MAX_COUNT; z++)
      {
        if (errorData[z].id < minId)
        {
          index = z;
          minId = errorData[z].id;
        }
      }
    }
  }

  // init error data
  errorData[index].id = errorDataId;
  z = 0;
  i = 0;
  while ((z < strlen(text)) && (i < $ERROR_MAX_TEXT_LENGTH-1))
  {
    if (!iscntrl(text[z])) { errorData[index].text[i] = text[z]; i++; }
    z++;
  }
  errorData[index].text[i] = '\\0';
  #ifndef NDEBUG
    errorData[index].fileName = fileName;
    errorData[index].lineNb   = lineNb;
  #endif /* not NDEBUG */

  return index+1;
}

";
}
if ($hFileName ne "")
{
  open(HFILE_HANDLE,"> $hFileName");
  print HFILE_HANDLE "\
#ifndef __ERRORS__
#define __ERRORS__

/***********************************************************************\
* Name   : ERROR_
* Purpose: create error
* Input  : code  - error code; see ERROR_...
*          errno - errno or 0
* Output : -
* Return : error
* Notes  : -
\***********************************************************************/

#define ERROR_(code,errno) Error_((ERROR_ ## code),errno)

/***********************************************************************\
* Name   : ERRORX_
* Purpose: create extended error
* Input  : code   - error code; see ERROR_...
*          errno  - errno or 0
*          format - format string (like printf)
*          ...    - optional arguments for format string
* Output : -
* Return : error
* Notes  : -
\***********************************************************************/

#define ERRORX_(code,errno,format,...) Errorx_((ERROR_ ## code),errno,format, ## __VA_ARGS__)

/***********************************************************************\
* Name   : ERRORF_
* Purpose: create error from existing error (update text)
* Input  : error  - error
*          format - format string (like printf)
*          ...    - optional arguments for format string
* Output : -
* Return : error
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define ERRORF_(error,format,...)      ((Errors)(  ((error) & ($ERROR_CODE_MASK|$ERROR_ERRNO_MASK)) \\
                                                   | ((_Error_dataToIndex(__FILE__,__LINE__,format, ## __VA_ARGS__) << $ERROR_DATAINDEX_SHIFT) & $ERROR_DATAINDEX_MASK) \\
                                                  ) \\
                                         )
#else
  #define ERRORF_(error,format,...)      ((Errors)(  ((error) & ($ERROR_CODE_MASK|$ERROR_ERRNO_MASK)) \\
                                                   | ((_Error_dataToIndex(format, ## __VA_ARGS__) << $ERROR_DATAINDEX_SHIFT) & $ERROR_DATAINDEX_MASK) \\
                                                  ) \\
                                         )
#endif

/***********************************************************************\
* Name   : Error_
* Purpose: create error
* Input  : code  - error code; see ERROR_...
*          errno - errno or 0
* Output : -
* Return : error
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define Error_(code,errno)             ((Errors)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                   | ((_Error_dataToIndex(__FILE__,__LINE__,NULL) << $ERROR_DATAINDEX_SHIFT) & $ERROR_DATAINDEX_MASK) \\
                                                   | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                  ) \\
                                         )
#else
  #define Error_(code,errno)             ((Errors)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                   | ((_Error_dataToIndex(NULL) << $ERROR_DATAINDEX_SHIFT) & $ERROR_DATAINDEX_MASK) \\
                                                   | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                  ) \\
                                         )
#endif

/***********************************************************************\
* Name   : Errorx_
* Purpose: create extended error
* Input  : code   - error code; see ERROR_...
*          errno  - errno or 0
*          format - format string (like printf)
*          ...    - optional arguments for format string
* Output : -
* Return : error
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define Errorx_(code,errno,format,...) ((Errors)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                   | ((_Error_dataToIndex(__FILE__,__LINE__,format, ## __VA_ARGS__) << $ERROR_DATAINDEX_SHIFT) & $ERROR_DATAINDEX_MASK) \\
                                                   | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                  ) \\
                                         )
#else
  #define Errorx_(code,errno,format,...) ((Errors)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                   | ((_Error_dataToIndex(format, ## __VA_ARGS__) << $ERROR_DATAINDEX_SHIFT) & $ERROR_DATAINDEX_MASK) \\
                                                   | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                  ) \\
                                         )
#endif

";
  writeHPrefix();
}
if ($javaFileName ne "")
{
  open(JAVAFILE_HANDLE,"> $javaFileName");
  writeJavaPrefix();
}

my @names;
my $defaultText;
my $line;
my $lineNb=0;
my $writeCPrefixFlag=0;
while ($line=<STDIN>)
{
  chop $line;
  $lineNb++;
  if (($line =~ /^\s*$/) || ($line =~ /^\s*\/\//)) { next; }
#print "$line\n";

  if    ($line =~ /^ERROR\s+(\w+)\s+"(.*)"\s*$/)
  {
    # error <name> <text>
    my $name=$1;
    my $text=$2;
    $errorNumber++;
    writeHFile("  $PREFIX$name = $errorNumber,\n");
    writeJavaFile("  static final int $name = $errorNumber;\n");
    if (!$writeCPrefixFlag) { writeCPrefix(); $writeCPrefixFlag = 1; }
    writeCFile("    case $PREFIX$name: stringSet(errorText,\"$text\",sizeof(errorText)); break;\n");
#TODO: #define ERROR_xxx Error_(ERROR_xxx,0)
  }
  elsif ($line =~ /^ERROR\s+(\w+)\s+(\S.*)\s*$/)
  {
    # error <name> <function>
    my $name    =$1;
    my $function=$2;
    $errorNumber++;
    writeHFile("  $PREFIX$name = $errorNumber,\n");
    writeJavaFile("  static final int $name = $errorNumber;\n");
    if (!$writeCPrefixFlag) { writeCPrefix(); $writeCPrefixFlag = 1; }
    writeCFile("    case $PREFIX$name: stringSet(errorText,$function,sizeof(errorText)); break;\n");
#TODO: #define ERROR_xxx Error_(ERROR_xxx,0)
  }
  elsif ($line =~ /^ERROR\s+(\w+)\s*$/)
  {
    # error <name>
    my $name=$1;
    $errorNumber++;
    writeHFile("  $PREFIX$name = $errorNumber,\n");
    writeJavaFile("  static final int $name = $errorNumber;\n");
    push(@names,$name);
  }
  elsif ($line =~ /^INCLUDE\s+"(.*)"\s*$/)
  {
    # include "file"
    my $file=$1;
    writeCFile("#include \"$file\"\n");
  }
  elsif ($line =~ /^INCLUDE\s+<(.*)>\s*$/)
  {
    # include <file>
    my $file=$1;
    writeCFile("#include <$file>\n");
  }
  elsif ($line =~ /^NONE\s+"(.*)"\s*$/)
  {
    # none <text>
    my $text=$1;
    if (!$writeCPrefixFlag) { writeCPrefix(); $writeCPrefixFlag = 1; }
    writeCFile("    case ".$PREFIX."NONE: stringSet(errorText,\"$text\",sizeof(errorText)); break;\n");
  }
  elsif ($line =~ /^DEFAULT\s+"(.*)"\s*$/)
  {
    $defaultText=$1;
  }
  elsif ($line =~ /^\s*#/)
  {
    writeCFile("$line\n");
  }
  else
  {
    # code
    if (scalar(@names) <= 0)
    {
      print STDERR "ERROR: Unknown data '$line' in line $lineNb\n";
      exit 1;
    }

    if (!$writeCPrefixFlag) { writeCPrefix(); $writeCPrefixFlag = 1; }
    foreach my $z (@names)
    {
      writeCFile("    case $PREFIX$z:\n");
    }
    @names=();
    writeCFile("      {\n");
    writeCFile("      $line\n");
    while ($line=<STDIN>)
    {
      chop $line;
      $lineNb++;
      if ($line =~ /^\s*$/) { last; }

      writeCFile("      $line\n");
    }
    writeCFile("      }\n");
    writeCFile("      break;\n");
  }
}

if ($cFileName ne "")
{
  writeCPostfix();
  close(CFILE_HANDLE);
}
if ($hFileName ne "")
{
  writeHPostfix();
  close(HFILE_HANDLE);
}
if ($javaFileName ne "")
{
  writeJavaPostfix();
  close(JAVAFILE_HANDLE);
}

exit 0;
# end of file
