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

my $ERROR_CODE_MASK            = "0x000003FF";
my $ERROR_CODE_SHIFT           = 0;
my $ERROR_DATA_INDEX_MASK      = "0x0000FC00";
my $ERROR_DATA_INDEX_SHIFT     = 10;
my $ERROR_ERRNO_MASK           = "0xFFFF0000";
my $ERROR_ERRNO_SHIFT          = 16;

my $ERROR_MAX_TEXT_LENGTH      = 2048;
my $ERROR_DATA_INDEX_MAX_COUNT = 63;

my $PREFIX                     = "ERROR_";

my $cFileName,$hFileName,$javaFileName;
my $javaClassName              = "Error";
my $trName                     = "tr";
my $help                       = 0;

my $errorNumber                = 0;
my @cHeader;
my @c;
my @hHeader;
my @h;
my @javaHeader;
my @java1;
my @java2;

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

#***********************************************************************
# Name   : expandC
# Purpose: expand C code
# Input  : string
# Output : -
# Return : string
# Notes  : -
#***********************************************************************

sub expandC($)
{
  my $s=shift(@_);
  
  if ($s =~ /^(\s*)STRING\s+(\S*)\s*\[\s*(\d+)\s*\]\s*;\s*/)
  {
    $s=$1."char ".$2."[".$3."];";
  }
  else
  {
    $s =~ s/TR\((.*)\)/$trName(\1)/g;
  }
  
  return $s;
}

#***********************************************************************
# Name   : expandJava
# Purpose: expand Java code
# Input  : string
# Output : -
# Return : string
# Notes  : -
#***********************************************************************

sub expandJava($)
{
  my $s=shift(@_);
  
  if ($s =~ /^(\s*)STRING\s+(\S*)\s*\[\s*(\d+)\s*\]\s*;\s*/)
  {
    $s=$1."StringBuilder ".$2." =  new StringBuilder();";
  }
  else
  {
    $s =~ s/ERROR_DATA/errorData/g;
    $s =~ s/ERROR_ERRNO_TEXT/Integer.toString(errno)/g;
    $s =~ s/ERROR_ERRNO/errno/g;
    $s =~ s/TR\((.*)\)/$trName(\1)/g;
  }
  
  return $s;
}

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

  push(@c,expandC($s));
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

  push(@h,expandC($s));
}

#***********************************************************************
# Name   : writeJavaHeader
# Purpose: write Java header
# Input  : string
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeJavaHeader($)
{
  my $s=shift(@_);

  push(@javaHeader,$s);
}

#***********************************************************************
# Name   : writeJava1
# Purpose: write Java 1
# Input  : string
#          1 for expand string
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeJava1($)
{
  my $s=shift(@_);
  my $expandFlag=shift(@_); if ($expandFlag eq "") { $expandFlag=0; }

  if ($expandFlag)
  {
    push(@java1,expandJava($s));
  }
  else
  {
    push(@java1,$s);
  }
}

#***********************************************************************
# Name   : writeJava2
# Purpose: write Java 2
# Input  : string
#          1 for expand string
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeJava2($_)
{
  my $s=shift(@_);
  my $expandFlag=shift(@_); if ($expandFlag eq "") { $expandFlag=0; }

  if ($expandFlag)
  {
    push(@java2,expandJava($s));
  }
  else
  {
    push(@java2,$s);
  }
}

#***********************************************************************
# Name   : writeH
# Purpose: write header file
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeH()
{
  foreach my $s (@hHeader)
  {
    print HFILE_HANDLE "$s\n";
  }

  # --------------------------------------------------------------------

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
                                                   | ((_Error_dataToIndex(__FILE__,__LINE__,format, ## __VA_ARGS__) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
                                                  ) \\
                                         )
#else
  #define ERRORF_(error,format,...)      ((Errors)(  ((error) & ($ERROR_CODE_MASK|$ERROR_ERRNO_MASK)) \\
                                                   | ((_Error_dataToIndex(format, ## __VA_ARGS__) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
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
                                                   | ((_Error_dataToIndex(__FILE__,__LINE__,NULL) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
                                                   | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                  ) \\
                                         )
#else
  #define Error_(code,errno)             ((Errors)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                   | ((_Error_dataToIndex(NULL) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
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
                                                   | ((_Error_dataToIndex(__FILE__,__LINE__,format, ## __VA_ARGS__) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
                                                   | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                  ) \\
                                         )
#else
  #define Errorx_(code,errno,format,...) ((Errors)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                   | ((_Error_dataToIndex(format, ## __VA_ARGS__) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
                                                   | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                  ) \\
                                         )
#endif

typedef enum
{
  ".$PREFIX."NONE = 0,
";

  # --------------------------------------------------------------------

  foreach my $s (@h)
  {
    print HFILE_HANDLE "$s\n";
  }

  # --------------------------------------------------------------------

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
* Name   : Error_getData
* Purpose: get data
* Input  : error - error
* Output : -
* Return : data
* Notes  : -
\***********************************************************************/

const char *Error_getData(Errors error);

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
# Name   : writeC
# Purpose: write C file
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeC()
{
  foreach my $s (@cHeader)
  {
    print CFILE_HANDLE "$s\n";
  }

  # --------------------------------------------------------------------

  print CFILE_HANDLE "\
#define __ERROR_IMPLEMENTATION__

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

static ErrorData errorData[$ERROR_DATA_INDEX_MAX_COUNT];   // last error data
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
    if (errorDataCount < $ERROR_DATA_INDEX_MAX_COUNT)
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
      for (z = 0; z < $ERROR_DATA_INDEX_MAX_COUNT; z++)
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

#define ERROR_GET_CODE(error)        (((error) & $ERROR_CODE_MASK) >> $ERROR_CODE_SHIFT)
#define ERROR_GET_CODE_TEXT(error)   Error_getCodeText(error)
#define ERROR_GET_DATA_INDEX(error)  (((error) & $ERROR_DATA_INDEX_MASK) >> $ERROR_DATA_INDEX_SHIFT)
#ifndef NDEBUG
#define ERROR_GET_FILENAME(error)    ((ERROR_GET_DATA_INDEX(error) > 0) ? errorData[ERROR_GET_DATA_INDEX(error)-1].fileName : NONE)
#define ERROR_GET_LINENB(error)      ((ERROR_GET_DATA_INDEX(error) > 0) ? errorData[ERROR_GET_DATA_INDEX(error)-1].lineNb : 0)
#define ERROR_GET_LINENB_TEXT(error) Error_getLineNbText(error)
#else
#define ERROR_GET_FILENAME(error)    NONE
#define ERROR_GET_LINENB(error)      0
#define ERROR_GET_LINENB_TEXT(error) NONE
#endif
#define ERROR_GET_DATA(error)        ((ERROR_GET_DATA_INDEX(error) > 0) ? errorData[ERROR_GET_DATA_INDEX(error)-1].text : NONE)
#define ERROR_GET_ERRNO(error)       ((int)((error) & $ERROR_ERRNO_MASK) >> $ERROR_ERRNO_SHIFT)
#define ERROR_GET_ERRNO_TEXT(error)  Error_getErrnoText(error)

#define ERROR_CODE        ERROR_GET_CODE(error)
#define ERROR_FILENAME    ERROR_GET_FILENAME(error)
#define ERROR_LINENB      ERROR_GET_LINENB(error)
#define ERROR_LINENB_TEXT ERROR_GET_LINENB_TEXT(error)
#define ERROR_DATA        ERROR_GET_DATA(error)
#define ERROR_ERRNO       ERROR_GET_ERRNO(error)
#define ERROR_ERRNO_TEXT  ERROR_GET_ERRNO_TEXT(error)

unsigned int Error_getCode(Errors error)
{
  return ERROR_GET_CODE(error);
}

const char *Error_getCodeText(Errors error)
{
  static char codeText[2+3+1];

  snprintf(codeText,sizeof(codeText)-1,\"0x%03x\",ERROR_GET_CODE(error));
  codeText[sizeof(codeText)-1] = '\\0';

  return codeText;
}

const char *Error_getData(Errors error)
{
  return ERROR_GET_DATA(error);
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
    static char lineNbText[16+1];

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
    static char locationText[PATH_MAX+2+16+1];

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
  static char errnoText[16+1];

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

  # --------------------------------------------------------------------

  foreach my $s (@c)
  {
    print CFILE_HANDLE "$s\n";
  }

  # --------------------------------------------------------------------

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
# Name   : writeJava
# Purpose: write Java file
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeJava()
{
  foreach my $s (@javaHeader)
  {
    print JAVAFILE_HANDLE "$s\n";
  }
  print JAVAFILE_HANDLE "\
class $javaClassName extends Exception
{
  public final static int NONE = 0;
";
  foreach my $s (@java1)
  {
    print JAVAFILE_HANDLE "$s\n";
  }
  print JAVAFILE_HANDLE "  public final static int UNKNOWN = ".($errorNumber+1).";

  public final int    code;
  public final int    errno;
  public final String data;

  /** get error code
   * @param error error
   * @return error code
   */
  public static int getCode($javaClassName error)
  {
    return error.code;
  }

  /** get errno
   * @param error error
   * @return errno
   */
  public static int getErrno($javaClassName error)
  {
    return error.errno;
  }

  /** get error data
   * @param error error
   * @return error data
   */
  public static String getData($javaClassName error)
  {
    return error.data;
  }

  /** get formated error text
   * @param error error
   * @return formated error text
   */
  public static String getText(int errorCode, int errno, String errorData)
  {
    StringBuilder errorText = new StringBuilder();

    switch (errorCode)
    {
";
  foreach my $s (@java2)
  {
    print JAVAFILE_HANDLE "$s\n";
  }
  print JAVAFILE_HANDLE "    }
    
    return errorText.toString();
  }
  
  /** get formated error text
   * @param error error
   * @return formated error text
   */
  public static String getText($javaClassName error)
  {
    return getText(getCode(error),getErrno(error),getData(error));
  }

  /** create error
   * @param errorCode error code
   * @param errno errno
   * @param errorData error data
   */
  $javaClassName(int errorCode, int errno, String errorData)
  {
    this.code  = errorCode;
    this.errno = errno;
    this.data  = errorData;
  }

  /** create error
   * @param errorCode error code
   * @param errorData error data
   */
  $javaClassName(int errorCode, String errorData)
  {
    this(errorCode,0,errorData);
  }

  /** create error
   * @param errorCode error code
   */
  $javaClassName(int errorCode)
  {
    this(errorCode,(String)null);
  }

  /** create error
   * @param errorCode error code
   * @param errno errno
   * @param format format string
   * @param arguments optional arguments
   */
  $javaClassName(int errorCode, int errno, String format, Object... arguments)
  {
    this(errorCode,errno,String.format(format,arguments));
  }

  /** get error code
   * @return error code
   */
  public int getCode()
  {
    return code;
  }

  /** get error errno
   * @return error errno
   */
  public int getErrno()
  {
    return errno;
  }

  /** get error data
   * @return error data
   */
  public String getData()
  {
    return data;
  }

  /** get error text
   * @return error text
   */
  public String getText()
  {
    return getText(this);
  }
  
  /** convert to string
   * @return string
   */
  public String toString()
  {
    return \"Error { \"+code+\", \"+data+\" }\";
  }
  
  // -------------------------------------------------------------------

  private static int sizeof(StringBuilder buffer)
  {
    return 0;
  }

  private static int sizeof(String buffer)
  {
    return 0;
  }

  private static String strerror(int n)
  {
    return \"\";
  }

  private static void stringSet(StringBuilder buffer, String text, int size)
  {
    buffer.setLength(0);
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, StringBuilder text, int size)
  {
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, String text, int size)
  {
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, int n, int size)
  {
    buffer.append(String.format(\"%03x\",n));
  }

  private static void stringFormat(StringBuilder buffer, int size, String format, Object... arguments)
  {
    buffer.append(String.format(format,arguments));
  }

  private static boolean stringIsEmpty(String string)
  {
    return string.isEmpty();
  }
}
";
}

#***********************************************************************
# Name   : writeFiles
# Purpose: write files
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeFiles()
{
  if ($hFileName ne "")
  {
    writeH();
  }
  if ($cFileName ne "")
  {
    writeC();
  }
  if ($javaFileName ne "")
  {
    writeJava();
  }
}

# ------------------------------ main program  -------------------------------

# get options
GetOptions("c=s" => \$cFileName,
           "h=s" => \$hFileName,
           "j=s" => \$javaFileName,
           "java-class-name=s" => \$javaClassName,
           "tr=s" => \$trName,
           "help" => \$help
          );

# help
if ($help == 1)
{
  print "Usage: $0 <options>\n";
  print "\n";
  print "Options: -c <file name>            - create C source file\n";
  print "         -h <file name>            - create C header file\n";
  print "         -j <file name>            - create Java file\n";
  print "         --java-class-name <name>  - Java class name (default: $javaClassName)\n";
  print "         --tr <name>               - name for tr-function (default: $trName)\n";
  print "         --help                    - output this help\n";
  exit 0;
}

# open files
if ($cFileName ne "")
{
  open(CFILE_HANDLE,"> $cFileName");
}
if ($hFileName ne "")
{
  open(HFILE_HANDLE,"> $hFileName");
}
if ($javaFileName ne "")
{
  open(JAVAFILE_HANDLE,"> $javaFileName");
}

# parse+generate
my @names;
my $defaultText;
my $line;
my $lineNb=0;
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
    writeHFile("  $PREFIX$name = $errorNumber,");
    writeJava1("  public final static int $name = $errorNumber;");
    writeCFile("    case $PREFIX$name:");
    writeCFile("      stringSet(errorText,\"$text\",sizeof(errorText));");
    writeCFile("      break;");
    writeJava2("      case $name:");
    writeJava2("        stringSet(errorText,\"$text\",sizeof(errorText));");
    writeJava2("        break;");
#TODO: #define ERROR_xxx Error_(ERROR_xxx,0)
  }
  elsif ($line =~ /^ERROR\s+(\w+)\s+(\S.*)\s*$/)
  {
    # error <name> <function>
    my $name    =$1;
    my $function=$2;
    $errorNumber++;
    writeHFile("  $PREFIX$name = $errorNumber,");
    writeJava1("  public final static int $name = $errorNumber;");
    writeCFile("    case $PREFIX$name:");
    writeCFile("      stringSet(errorText,$function,sizeof(errorText));");
    writeCFile("      break;");
    writeJava2("      case $name:");
    writeJava2("        stringSet(errorText,$function,sizeof(errorText));",1);
    writeJava2("        break;");
#TODO: #define ERROR_xxx Error_(ERROR_xxx,0)
  }
  elsif ($line =~ /^ERROR\s+(\w+)\s*$/)
  {
    # error <name>
    my $name=$1;
    $errorNumber++;
    writeHFile("  $PREFIX$name = $errorNumber,");
    writeJava1("  public final static int $name = $errorNumber;");
    push(@names,$name);
  }
  elsif ($line =~ /^INCLUDE\s+"(.*)"\s*$/)
  {
    # include "file"
    my $file=$1;
    writeCFile("#include \"$file\"");
  }
  elsif ($line =~ /^INCLUDE\s+<(.*)>\s*$/)
  {
    # include <file>
    my $file=$1;
    writeCFile("#include <$file>");
  }
  elsif ($line =~ /^IMPORT\s+(.*)\s*$/)
  {
    # import package
    my $package=$1;
    writeJavaHeader("import $package;");
  }
  elsif ($line =~ /^NONE\s+"(.*)"\s*$/)
  {
    # none <text>
    my $text=$1;
    writeCFile("    case ".$PREFIX."NONE: stringSet(errorText,\"$text\",sizeof(errorText)); break;");
  }
  elsif ($line =~ /^DEFAULT\s+"(.*)"\s*$/)
  {
    $defaultText=$1;
  }
  elsif ($line =~ /^\s*#/)
  {
    # C preprocessor
    writeCFile("$line");
  }
  else
  {
    # code
    if (scalar(@names) <= 0)
    {
      print STDERR "ERROR: Unknown data '$line' in line $lineNb";
      exit 1;
    }

    foreach my $s (@names)
    {
      writeCFile("    case $PREFIX$s:");
      writeJava2("      case $s:");
    }
    writeCFile("      {");
    writeCFile("      $line");
    writeJava2("      $line",1);
    while ($line=<STDIN>)
    {
      chop $line;
      $lineNb++;
      if ($line =~ /^\s*$/) { last; }

      writeCFile("      $line");
      writeJava2("      $line",1);
    }
    writeCFile("      }");
    writeJava2("        break;");

    @names=();
  }
}

# write files
writeFiles();

# close files
if ($cFileName ne "")
{
  close(CFILE_HANDLE);
}
if ($hFileName ne "")
{
  close(HFILE_HANDLE);
}
if ($javaFileName ne "")
{
  close(JAVAFILE_HANDLE);
}

exit 0;
# end of file
