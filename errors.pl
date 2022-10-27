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

my $ERROR_CODE_MASK             = "0x000003FF";
my $ERROR_CODE_SHIFT            = 0;
my $ERROR_DATA_INDEX_MASK       = "0x0000FC00";
my $ERROR_DATA_INDEX_SHIFT      = 10;
my $ERROR_ERRNO_MASK            = "0xFFFF0000";
my $ERROR_ERRNO_SHIFT           = 16;

my $ERROR_MAX_TEXT_LENGTH       = 2048;
my $ERROR_MAX_TEXT_LENGTH_DEBUG = 4096;
my $ERROR_DATA_INDEX_MAX_COUNT  = 63;

my $PREFIX                      = "ERROR_CODE_";

my $cFileName,$hFileName,$javaFileName;
my $javaClassName               = "Error";
my $trName                      = "tr";
my $help                        = 0;

my $errorNumber                 = 0;

my @errorNames;
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
    $s =~ s/ERROR_ERRNO/errno/g;
    if ($s =~ /^(.*)TR\((.*)\)(.*)$/)
    {
      my $prefix =$1;
      my $text   =$2;
      my $postfix=$3;

      $text =~ s/'/''/g;

      $s = $prefix.$trName."(".$text.")".$postfix;
    }
  }

  return $s;
}

#***********************************************************************
# Name   : writeCFileHeader
# Purpose: write C file header
# Input  : string
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub writeCFileHeader($)
{
  my $s=shift(@_);

  push(@cHeader,$s);
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

  print HFILE_HANDLE "/* This file is auto-generated by errors.pl. Do NOT edit! */

#ifndef __ERRORS__
#define __ERRORS__

#include <stdint.h>

/***********************************************************************\
* Name   : ERROR_
* Purpose: create error
* Input  : code  - error code; see ERROR_...
*          errno - errno or 0
* Output : -
* Return : error
* Notes  : -
\***********************************************************************/

#define ERROR_(code,errno) Error_((".$PREFIX." ## code),errno)

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

#ifndef NDEBUG
  #define ERRORX_(code,errno,format,...) Errorx_(__FILE__,__LINE__,(".$PREFIX." ## code),errno,format, ## __VA_ARGS__)
#else
  #define ERRORX_(code,errno,format,...) Errorx_((".$PREFIX." ## code),errno,format, ## __VA_ARGS__)
#endif

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
  #define ERRORF_(error,format,...)      ((Errors)(intptr_t)(  ((error) & ($ERROR_CODE_MASK|$ERROR_ERRNO_MASK)) \\
                                                             | ((_Error_dataToIndex(__FILE__,__LINE__,format, ## __VA_ARGS__) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
                                                            ) \\
                                         )
#else
  #define ERRORF_(error,format,...)      ((Errors)(intptr_t)(  ((error) & ($ERROR_CODE_MASK|$ERROR_ERRNO_MASK)) \\
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
  #define Error_(code,errno)             ((Errors)(intptr_t)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                             | ((_Error_dataToIndex(__FILE__,__LINE__,NULL) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
                                                             | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                            ) \\
                                         )
#else
  #define Error_(code,errno)             ((Errors)(intptr_t)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                             | ((_Error_dataToIndex(NULL) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
                                                             | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                            ) \\
                                         )
#endif

/***********************************************************************\
* Name   : Errorx_
* Purpose: create extended error
* Input  : fileName - file name
*          lineNb   - line number
*          code     - error code; see ERROR_...
*          errno    - errno or 0
*          format   - format string (like printf)
*          ...      - optional arguments for format string
* Output : -
* Return : error
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define Errorx_(fileName,lineNb,code,errno,format,...) ((Errors)(intptr_t)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
                                                                             | ((_Error_dataToIndex(fileName,lineNb,format, ## __VA_ARGS__) << $ERROR_DATA_INDEX_SHIFT) & $ERROR_DATA_INDEX_MASK) \\
                                                                             | (((code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK) \\
                                                                            ) \\
                                                         )
#else
  #define Errorx_(code,errno,format,...) ((Errors)(intptr_t)(  (((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) \\
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
} ErrorCodes;

// special errors type
typedef intptr_t* Errors;

// error macros
";

  print HFILE_HANDLE "#define ERROR_NONE (Errors)(".$PREFIX."NONE & ($ERROR_CODE_MASK|$ERROR_ERRNO_MASK))\n";
  foreach my $s (@errorNames)
  {
    print HFILE_HANDLE "#define ERROR_$s Error_(".$PREFIX."$s,0)\n";
  }
  print HFILE_HANDLE "#define ERROR_UNKNOWN (Errors)(".$PREFIX."UNKNOWN & ($ERROR_CODE_MASK|$ERROR_ERRNO_MASK))\n";

  print HFILE_HANDLE "\
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
*          additional format specifiers:
*            %E convert errno to text (with lower case start)
\***********************************************************************/

#ifndef NDEBUG
int _Error_dataToIndex(const char *fileName, unsigned long lineNb, const char *format, ...);
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
* Name   : Error_getErrno
* Purpose: get errno
* Input  : error - error
* Output : -
* Return : errno
* Notes  : -
\***********************************************************************/

int Error_getErrno(Errors error);

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
  print CFILE_HANDLE "/* This file is auto-generated by errors.pl. Do NOT edit! */

";
  foreach my $s (@cHeader)
  {
    print CFILE_HANDLE "$s\n";
  }

  # --------------------------------------------------------------------

  print CFILE_HANDLE "\
#define __ERROR_IMPLEMENTATION__

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>

#include \"common/global.h\"

#include \"errors.h\"

#ifdef NDEBUG
  #define ERROR_MAX_TEXT_LENGTH $ERROR_MAX_TEXT_LENGTH
#else
  #define ERROR_MAX_TEXT_LENGTH $ERROR_MAX_TEXT_LENGTH_DEBUG
#endif

// use NONE to avoid warning in strn*-functions which do not accept NULL (this case must be checked before calling strn*)
static const char *NONE = NULL;

typedef struct
{
  int  id;
  char text[ERROR_MAX_TEXT_LENGTH];
  #ifndef NDEBUG
    const char   *fileName;
    unsigned int lineNb;
  #endif /* not NDEBUG */
} ErrorData;

static pthread_mutex_t errorTextLock  = PTHREAD_MUTEX_INITIALIZER;
static ErrorData       errorData[$ERROR_DATA_INDEX_MAX_COUNT];   // last error data
static uint            errorDataCount = 0;                       // last error data count (=max. when all data entries are used; recycle oldest entry if required)
static uint            errorDataId    = 0;                       // total number of error data

LOCAL void vformatErrorText(char *string, ulong n, const char *format, va_list arguments)
{
  bool       longFlag,longLongFlag;
  char       quoteFlag;
  union
  {
    bool       b;
    int        i;
    uint       ui;
    long       l;
    ulong      ul;
    int64      ll;
    uint64     ull;
    float      f;
    double     d;
    char       ch;
    const char *s;
    void       *p;
  } value;
  const char *t;
  char       buffer[256];

  stringClear(string);
  while (!stringIsEmpty(format))
  {
    switch (stringAt(format,0))
    {
      case '\\\\':
        // escaped character
        stringAppendChar(string,n,'\\\\');
        format++;
        if (!stringIsEmpty(format))
        {
          stringAppendChar(string,n,stringAt(format,0));
          format++;
        }
        break;
      case '%':
        // format character
        format++;

        // check for longlong/long flag
        longLongFlag = FALSE;
        longFlag     = FALSE;
        if (stringAt(format,0) == 'l')
        {
          format++;
          if (stringAt(format,0) == 'l')
          {
            format++;
            longLongFlag = TRUE;
          }
          else
          {
            longFlag = TRUE;
          }
        }

        // quoting flag (ignore quote char)
        if (   !stringIsEmpty(format)
            && !isalpha(stringAt(format,0))
            && (stringAt(format,0) != '%')
            && (   (stringAt(format,1) == 's')
                || (stringAt(format,1) == 'E')
               )
           )
        {
          quoteFlag = TRUE;
          format++;
        }
        else
        {
          quoteFlag = FALSE;
        }

        if (!stringIsEmpty(format))
        {
          // handle format type
          switch (stringAt(format,0))
          {
            case 'b':
              // boolean
              format++;

              value.i = va_arg(arguments,int);
              stringAppend(string,n,(value.i != 0) ? \"true\" : \"false\");
              break;
            case 'd':
              // integer
              format++;

              if      (longLongFlag)
              {
                value.ll = va_arg(arguments,int64);
                stringFormatAppend(string,n,\"%\"PRIi64,value.ll);
              }
              else if (longFlag)
              {
                value.l = va_arg(arguments,int64);
                stringFormatAppend(string,n,\"%ld\",value.l);
              }
              else
              {
                value.i = va_arg(arguments,int);
                stringFormatAppend(string,n,\"%d\",value.i);
              }
              break;
            case 'u':
              // unsigned integer
              format++;

              if      (longLongFlag)
              {
                value.ull = va_arg(arguments,uint64);
                stringFormatAppend(string,n,\"%\"PRIu64,value.ull);
              }
              else if (longFlag)
              {
                value.ul = va_arg(arguments,ulong);
                stringFormatAppend(string,n,\"%lu\",value.ul);
              }
              else
              {
                value.ui = va_arg(arguments,uint);
                stringFormatAppend(string,n,\"%u\",value.ui);
              }
              break;
            case 'f':
              // float/double
              format++;

              if (longFlag)
              {
                value.d = va_arg(arguments,double);
                stringFormatAppend(string,n,\"%lf\",value.d);
              }
              else
              {
                value.f = (float)va_arg(arguments,double);
                stringFormatAppend(string,n,\"%lf\",value.f);
              }
              break;
            case 'c':
              // character
              format++;

              value.ch = (char)va_arg(arguments,int);
              stringAppendChar(string,n,value.ch);
              break;
            case 's':
              // string
              format++;

              value.s = va_arg(arguments,const char*);

              if (quoteFlag) stringAppendChar(string,n,'\\'');
              if (value.s != NULL)
              {
                t = value.s;
                while (!stringIsEmpty(t))
                {
                  switch (stringAt(t,0))
                  {
                    case '\\'':
                      if (quoteFlag)
                      {
                        stringAppend(string,n,\"''\");
                      }
                      else
                      {
                        stringAppendChar(string,n,'\\'');
                      }
                      break;
                    default:
                      stringAppendChar(string,n,stringAt(t,0));
                      break;
                  }
                  t++;
                }
              }
              if (quoteFlag) stringAppendChar(string,n,'\\'');
              break;
            case 'p':
              // pointer
              format++;

              value.p = va_arg(arguments,void*);
              stringFormatAppend(string,n,\"%p\",value.p);
              break;
            case 'E':
              // errno text
              format++;

              value.i = va_arg(arguments,int);

              // start with lower case
              stringSet(buffer,sizeof(buffer),strerror(value.i));
              if (!stringIsEmpty(buffer)) buffer[0] = tolower(buffer[0]);

              stringAppend(string,n,buffer);
              break;
            case '%':
              // %%
              format++;

              stringAppendChar(string,n,'%');
              break;
            default:
              stringAppendChar(string,n,'%');
              stringAppendChar(string,n,stringAt(format,0));
              break;
          }
        }
        break;
      default:
        stringAppendChar(string,n,stringAt(format,0));
        format++;
        break;
    }
  }
}

#ifndef NDEBUG
int _Error_dataToIndex(const char *fileName, ulong lineNb, const char *format, ...)
#else
int _Error_dataToIndex(const char *format, ...)
#endif
{
  va_list arguments;
  char    text[ERROR_MAX_TEXT_LENGTH];
  int     index;
  int     minId;
  uint    z,i;

  if (format != NULL)
  {
    // format error text
    va_start(arguments,format);
    vformatErrorText(text,sizeof(text),format,arguments);
    va_end(arguments);
  }
  else
  {
    stringClear(text);
  }

  pthread_mutex_lock(&errorTextLock);
  {
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
    while ((z < stringLength(text)) && (i < ERROR_MAX_TEXT_LENGTH-1))
    {
      if (!iscntrl(text[z])) { errorData[index].text[i] = text[z]; i++; }
      z++;
    }
    errorData[index].text[i] = '\\0';
    #ifndef NDEBUG
      errorData[index].fileName = fileName;
      errorData[index].lineNb   = lineNb;
    #endif /* not NDEBUG */
  }
  pthread_mutex_unlock(&errorTextLock);

  return index+1;
}

#define ERROR_GET_CODE(error)        ((((intptr_t)(error)) & $ERROR_CODE_MASK) >> $ERROR_CODE_SHIFT)
#define ERROR_GET_CODE_TEXT(error)   Error_getCodeText(error)
#define ERROR_GET_DATA_INDEX(error)  ((((intptr_t)(error)) & $ERROR_DATA_INDEX_MASK) >> $ERROR_DATA_INDEX_SHIFT)
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
#define ERROR_GET_ERRNO(error)       ((int)(((intptr_t)(error)) & $ERROR_ERRNO_MASK) >> $ERROR_ERRNO_SHIFT)
#define ERROR_GET_ERRNO_TEXT(error)  Error_getErrnoText(error)

#define ERROR_CODE        ERROR_GET_CODE(error)
#define ERROR_FILENAME    ERROR_GET_FILENAME(error)
#define ERROR_LINENB      ERROR_GET_LINENB(error)
#define ERROR_LINENB_TEXT ERROR_GET_LINENB_TEXT(error)
#define ERROR_DATA        ERROR_GET_DATA(error)
#define ERROR_ERRNO       ERROR_GET_ERRNO(error)

unsigned int Error_getCode(Errors error)
{
  return ERROR_GET_CODE(error);
}

const char *Error_getCodeText(Errors error)
{
  static char codeText[2+3+1];

  stringFormat(codeText,sizeof(codeText),\"0x%03x\",ERROR_GET_CODE(error));

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

    stringFormat(lineNbText,sizeof(lineNbText),\"%d\",ERROR_GET_LINENB(error));

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

    stringFormat(locationText,sizeof(locationText),\"%s, %d\",ERROR_GET_FILENAME(error),ERROR_GET_LINENB(error));

    return locationText;
  #else
    UNUSED_VARIABLE(error);

    return NULL;
  #endif
}

int Error_getErrno(Errors error)
{
  return ERROR_GET_ERRNO(error);
}

const char *Error_getErrnoText(Errors error)
{
  static char errnoText[16+1];

  stringFormat(errnoText,sizeof(errnoText),\"%d\",ERROR_GET_ERRNO(error));

  return errnoText;
}

const char *Error_getText(Errors error)
{
  static char errorText[ERROR_MAX_TEXT_LENGTH];

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
    writeCFile("    default: stringSet(errorText,sizeof(errorText),\"$defaultText\"); break;\n");
  }

  print CFILE_HANDLE "\
  }
  if (stringIsEmpty(errorText)) stringSet(errorText,sizeof(errorText),\"unknown\");
  #ifndef NDEBUG
    if (ERROR_FILENAME != NULL)
    {
      stringAppend(errorText,sizeof(errorText),\" at \");
      stringAppend(errorText,sizeof(errorText),ERROR_FILENAME);
      stringAppend(errorText,sizeof(errorText),\", \");
      stringAppend(errorText,sizeof(errorText),ERROR_LINENB_TEXT);
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
      case UNKNOWN:
        stringSet(errorText,sizeof(errorText),BARControl.tr(\"unknown\"));
        break;
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

  /** get error message
   * @return error message
   */
  public String getMessage()
  {
    return getText(this);
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
    return getText(this);
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

  private static void stringSet(StringBuilder buffer, int size, String text)
  {
    buffer.setLength(0);
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, int size, StringBuilder text)
  {
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, int size, String text)
  {
    buffer.append(text);
  }

  private static void stringAppend(StringBuilder buffer, int n, int size)
  {
    buffer.append(String.format(\"%03x\",n));
  }

  private static void stringFormat(StringBuilder buffer, int size, String format, Object... arguments)
  {
    buffer.setLength(0);
    buffer.append(String.format(format,arguments));
  }

  private static void stringFormatAppend(StringBuilder buffer, int size, String format, Object... arguments)
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
my $defaultText;
my @names;
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
    writeHFile("#line $lineNb \"errors.def\"");
    writeHFile("  $PREFIX$name = $errorNumber,");

    writeCFile("    case $PREFIX$name:");
    writeCFile("#line $lineNb \"errors.def\"");
    writeCFile("      stringSet(errorText,sizeof(errorText),\"$text\");");
    writeCFile("      break;");

    writeJava1("  public final static int $name = $errorNumber;");
    writeJava2("      case $name:");
    writeJava2("        stringSet(errorText,sizeof(errorText),\"$text\");");
    writeJava2("        break;");

    push(@errorNames,$name);
#TODO: #define ERROR_xxx Error_(ERROR_xxx,0)
  }
  elsif ($line =~ /^ERROR\s+(\w+)\s+(\S.*)\s*$/)
  {
    # error <name> <function>
    my $name    =$1;
    my $function=$2;
    $errorNumber++;
    writeHFile("#line $lineNb \"errors.def\"");
    writeHFile("  $PREFIX$name = $errorNumber,");


    writeCFile("    case $PREFIX$name:");
    writeCFile("#line $lineNb \"errors.def\"");
    writeCFile("      stringSet(errorText,sizeof(errorText),$function);");
    writeCFile("      break;");

    writeJava1("  public final static int $name = $errorNumber;");
    writeJava2("      case $name:");
    writeJava2("        stringSet(errorText,sizeof(errorText),$function);",1);
    writeJava2("        break;");

    push(@errorNames,$name);
#TODO: #define ERROR_xxx Error_(ERROR_xxx,0)
  }
  elsif ($line =~ /^ERROR\s+(\w+)\s*$/)
  {
    # error <name>
    my $name=$1;
    $errorNumber++;
    writeHFile("#line $lineNb \"errors.def\"");
    writeHFile("  $PREFIX$name = $errorNumber,");
    writeJava1("  public final static int $name = $errorNumber;");
    push(@names,$name);

    push(@errorNames,$name);
  }
  elsif ($line =~ /^INCLUDE\s+"(.*)"\s*$/)
  {
    # include "file"
    my $file=$1;
    writeCFileHeader("#line $lineNb \"errors.def\"");
    writeCFileHeader("#include \"$file\"");
  }
  elsif ($line =~ /^INCLUDE\s+<(.*)>\s*$/)
  {
    # include <file>
    my $file=$1;
    writeCFileHeader("#line $lineNb \"errors.def\"");
    writeCFileHeader("#include <$file>");
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
    writeCFile("#line $lineNb \"errors.def\"");
    writeCFile("    case ".$PREFIX."NONE: stringSet(errorText,sizeof(errorText),\"$text\"); break;");
  }
  elsif ($line =~ /^DEFAULT\s+"(.*)"\s*$/)
  {
    $defaultText=$1;
  }
  elsif ($line =~ /^\s*#/)
  {
    # C preprocessor
    writeCFileHeader("#line $lineNb \"errors.def\"");
    writeCFileHeader("$line");
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
      writeCFile("#line $lineNb \"errors.def\"");
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
    writeCFile("      break;");
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
