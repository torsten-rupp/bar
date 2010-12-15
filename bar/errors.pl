#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/errors.pl,v $
# $Revision: 1.14 $
# $Author: torsten $
# Contents: create header/c file definition from errors definition
# Systems: all
#
# ----------------------------------------------------------------------------

# syntax of error definition file
#
# INCLUDE "<file>"
# ERROR <name> "<text>"
# ERROR <name>
#   <code>
# DEFAULT "<text>"
# NONE "<text>"

# ---------------------------- additional packages ---------------------------
use English;
use POSIX;
use Getopt::Std;
use Getopt::Long;

# ---------------------------- constants/variables ---------------------------

my $ERROR_CODE_MASK           = "0x000003FF";
my $ERROR_CODE_SHIFT          = 0;
my $ERROR_TEXTINDEX_MASK      = "0x0000FC00";
my $ERROR_TEXTINDEX_SHIFT     = 10;
my $ERROR_ERRNO_MASK          = "0xFFFF0000";
my $ERROR_ERRNO_SHIFT         = 16;

my $MAX_ERRORTEXT_LENGTH      = 512;
my $ERROR_TEXTINDEX_MAX_COUNT = 63;;

my $PREFIX                    = "ERROR_";

my $cFileName,$hFileName,$javaFileName;
my $errorNumber=0;

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

sub writeCFile($)
{
  my $s=shift(@_);

  if ($cFileName ne "")
  {
    print CFILE_HANDLE $s;
  }
}

sub writeHFile($)
{
  my $s=shift(@_);

  if ($hFileName ne "")
  {
    print HFILE_HANDLE $s;
  }
}

sub writeJavaFile($)
{
  my $s=shift(@_);

  if ($javaFileName ne "")
  {
    print JAVAFILE_HANDLE $s;
  }
}

sub writeCPrefix()
{
  print CFILE_HANDLE "\
#define GET_ERROR_CODE(error)      (((error) & $ERROR_CODE_MASK) >> $ERROR_CODE_SHIFT)
#define GET_ERROR_TEXTINDEX(error) (((error) & $ERROR_TEXTINDEX_MASK) >> $ERROR_TEXTINDEX_SHIFT)
#define GET_ERROR_TEXT(error)      ((GET_ERROR_TEXTINDEX(error) > 0)?errorTexts[GET_ERROR_TEXTINDEX(error)-1].text:NONE)
#define GET_ERRNO(error)           ((int)((error) & $ERROR_ERRNO_MASK) >> $ERROR_ERRNO_SHIFT)

#define ERROR_CODE GET_ERROR_CODE(error)
#define ERROR_TEXT GET_ERROR_TEXT(error)
#define ERRNO      GET_ERRNO(error)

unsigned int Errors_getCode(Errors error)
{
  return GET_ERROR_CODE(error);
}

const char *Errors_getText(Errors error)
{
  static char errorText[$MAX_ERRORTEXT_LENGTH];

  strcpy(errorText,\"unknown\");
  switch (GET_ERROR_CODE(error))
  {
";
}

sub writeCPostfix()
{
  if ($defaultText ne "")
  {
    writeCFile("    default: return \"$defaultText\";\n");
  }
  print CFILE_HANDLE "\
  }

  return errorText;
}
";
}

sub writeHPrefix()
{
  print HFILE_HANDLE "\
typedef enum
{
  ".$PREFIX."NONE = 0,
";
}

sub writeHPostfix()
{
  print HFILE_HANDLE "\
  ".$PREFIX."UNKNOWN = ".($errorNumber+1)."
} Errors;

int _Errors_textToIndex(const char *text);
unsigned int Errors_getCode(Errors error);
const char *Errors_getText(Errors error);

#endif /* __ARCHIVE_FORMAT__ */
";
}

sub writeJavaPrefix()
{
  print JAVAFILE_HANDLE "class Errors\n";
  print JAVAFILE_HANDLE "{\n";
  print JAVAFILE_HANDLE "  static final int NONE = 0;\n";
}

sub writeJavaPostfix()
{
  print JAVAFILE_HANDLE "  static final int UNKNOWN = $errorNumber;\n";
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
  print CFILE_HANDLE "\
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include \"errors.h\"

// use NONE to avoid warning in strn*-functions which do not accept NULL (this case must be checked before calling s
static const char *NONE = NULL;

typedef struct
{
  int  id;
  char text[$MAX_ERRORTEXT_LENGTH];
} ErrorText;

static ErrorText errorTexts[$ERROR_TEXTINDEX_MAX_COUNT];
static int       errorTextCount = 0;
static int       errorTextId    = 0;

int _Errors_textToIndex(const char *text)
{
  int index;
  int minId;
  int z,i;

  if (text != NULL)
  {
    errorTextId++;
    if (errorTextCount < $ERROR_TEXTINDEX_MAX_COUNT)
    {
      index = errorTextCount;
      errorTextCount++;
    }
    else
    {
      index = 0;
      minId = INT_MAX;
      for (z = 0; z < $ERROR_TEXTINDEX_MAX_COUNT; z++)
      {
        if (errorTexts[z].id < minId)
        {
          index = z;
          minId = errorTexts[z].id;
        }
      }
    }
    z = 0;
    i = 0;
    while ((z < strlen(text)) && (i < $MAX_ERRORTEXT_LENGTH-1-1))
    {
      if (!iscntrl(text[z])) { errorTexts[index].text[i] = text[z]; i++; }
      z++;
    }
    errorTexts[index].text[i] = '\\0';
    errorTexts[errorTextCount].id = errorTextId;
    return index+1;
  }
  else
  {
    return 0;
  }
}

";
}
if ($hFileName ne "")
{
  open(HFILE_HANDLE,"> $hFileName");
  print HFILE_HANDLE "\
#ifndef __ERRORS__
#define __ERRORS__

#define ERROR(code,errno)       ((((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) |                                                                                   (((ERROR_ ## code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK))
#define ERRORX(code,errno,text) ((((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) | ((_Errors_textToIndex(text) << $ERROR_TEXTINDEX_SHIFT) & $ERROR_TEXTINDEX_MASK) | (((ERROR_ ## code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK))

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
    writeCFile("    case $PREFIX$name: return \"$text\";\n");
  }
  elsif ($line =~ /^ERROR\s+(\w+)\s+(.*)\s*$/)
  {
    # error <name> <function>
    my $name    =$1;
    my $function=$2;
    $errorNumber++;
    writeHFile("  $PREFIX$name = $errorNumber,\n");
    writeJavaFile("  static final int $name = $errorNumber;\n");
    if (!$writeCPrefixFlag) { writeCPrefix(); $writeCPrefixFlag = 1; }
    writeCFile("    case $PREFIX$name: return $function;\n");
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
    # include "<file>"
    my $file=$1;
    writeCFile("#include \"$file\"\n");
  }
  elsif ($line =~ /^NONE\s+"(.*)"\s*$/)
  {
    # none <text>
    my $text=$1;
    if (!$writeCPrefixFlag) { writeCPrefix(); $writeCPrefixFlag = 1; }
    writeCFile("    case ".$PREFIX."NONE: return \"$text\";\n");
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
      print STDERR "Unknown data '$line' in line $lineNb\n";
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
