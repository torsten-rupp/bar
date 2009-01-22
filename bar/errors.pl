#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/errors.pl,v $
# $Revision: 1.8 $
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

my $MAX_ERRORTEXT_LENGTH      = 128;
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
  print CFILE_HANDLE "#define GET_ERROR_CODE(error)      (((error) & $ERROR_CODE_MASK) >> $ERROR_CODE_SHIFT)\n";
  print CFILE_HANDLE "#define GET_ERROR_TEXTINDEX(error) (((error) & $ERROR_TEXTINDEX_MASK) >> $ERROR_TEXTINDEX_SHIFT)\n";
  print CFILE_HANDLE "#define GET_ERROR_TEXT(error)      ((GET_ERROR_TEXTINDEX(error)>0)?errorTexts[GET_ERROR_TEXTINDEX(error)-1].text:\"unknown\")\n";
  print CFILE_HANDLE "#define GET_ERRNO(error)           ((long)((error) & $ERROR_ERRNO_MASK) >> $ERROR_ERRNO_SHIFT)\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "#define ERROR_CPDE GET_ERROR_CODE(error)\n";
  print CFILE_HANDLE "#define ERROR_TEXT GET_ERROR_TEXT(error)\n";
  print CFILE_HANDLE "#define ERRNO      GET_ERRNO(error)\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "unsigned int Errors_getCode(Errors error)\n";
  print CFILE_HANDLE "{\n";
  print CFILE_HANDLE "  return GET_ERROR_CODE(error);\n";
  print CFILE_HANDLE "}\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "const char *Errors_getText(Errors error)\n";
  print CFILE_HANDLE "{\n";
  print CFILE_HANDLE "  static char errorText[$MAX_ERRORTEXT_LENGTH];\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "  strcpy(errorText,\"unknown\");\n";
  print CFILE_HANDLE "  switch (GET_ERROR_CODE(error))\n";
  print CFILE_HANDLE "  {\n";
}

sub writeCPostfix()
{
  if ($defaultText ne "")
  {
    writeCFile("    default: return \"$defaultText\";\n");
  }
  print CFILE_HANDLE "  }\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "  return errorText;\n";
  print CFILE_HANDLE "}\n";
}

sub writeHPrefix()
{
  print HFILE_HANDLE "typedef enum\n";
  print HFILE_HANDLE "{\n";
  print HFILE_HANDLE "  ".$PREFIX."NONE = 0,\n";
}

sub writeHPostfix()
{
  print HFILE_HANDLE "  ".$PREFIX."UNKNOWN = $errorNumber\n";
  print HFILE_HANDLE "} Errors;\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "const int _Errors_textToIndex(const char *text);\n";
  print HFILE_HANDLE "unsigned int Errors_getCode(Errors error);\n";
  print HFILE_HANDLE "const char *Errors_getText(Errors error);\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "#endif /* __ARCHIVE_FORMAT__ */\n";
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
  print CFILE_HANDLE "#include <stdlib.h>\n";
  print CFILE_HANDLE "#include <stdio.h>\n";
  print CFILE_HANDLE "#include <string.h>\n";
  print CFILE_HANDLE "#include <limits.h>\n";
  print CFILE_HANDLE "#include <ctype.h>\n";
  print CFILE_HANDLE "#include <errno.h>\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "#include \"errors.h\"\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "typedef struct\n";
  print CFILE_HANDLE "{\n";
  print CFILE_HANDLE "  int  id;\n";
  print CFILE_HANDLE "  char text[$MAX_ERRORTEXT_LENGTH];\n";
  print CFILE_HANDLE "} ErrorText;\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "static ErrorText errorTexts[$ERROR_TEXTINDEX_MAX_COUNT];\n";
  print CFILE_HANDLE "static int       errorTextCount = 0;\n";
  print CFILE_HANDLE "static int       errorTextId    = 0;\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "const int _Errors_textToIndex(const char *text)\n";
  print CFILE_HANDLE "{\n";
  print CFILE_HANDLE "  int index;\n";
  print CFILE_HANDLE "  int minId;\n";
  print CFILE_HANDLE "  int z,i;\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "  errorTextId++;\n";
  print CFILE_HANDLE "  if (errorTextCount < $ERROR_TEXTINDEX_MAX_COUNT)\n";
  print CFILE_HANDLE "  {\n";
  print CFILE_HANDLE "    index = errorTextCount;\n";
  print CFILE_HANDLE "    errorTextCount++;\n";
  print CFILE_HANDLE "  }\n";
  print CFILE_HANDLE "  else\n";
  print CFILE_HANDLE "  {\n";
  print CFILE_HANDLE "    index = 0;\n";
  print CFILE_HANDLE "    minId = INT_MAX;\n";
  print CFILE_HANDLE "    for (z = 0; z < $ERROR_TEXTINDEX_MAX_COUNT; z++)\n";
  print CFILE_HANDLE "    {\n";
  print CFILE_HANDLE "      if (errorTexts[z].id < minId)\n";
  print CFILE_HANDLE "      {\n";
  print CFILE_HANDLE "        index = z;\n";
  print CFILE_HANDLE "        minId = errorTexts[z].id;\n";
  print CFILE_HANDLE "      }\n";
  print CFILE_HANDLE "    }\n";
  print CFILE_HANDLE "  }\n";
  print CFILE_HANDLE "  z = 0;\n";
  print CFILE_HANDLE "  i = 0;\n";
  print CFILE_HANDLE "  while ((z < strlen(text)) && (i < $MAX_ERRORTEXT_LENGTH-1-1))\n";
  print CFILE_HANDLE "  {\n";
  print CFILE_HANDLE "    if (!iscntrl(text[z])) { errorTexts[index].text[i] = text[z]; i++; }\n";
  print CFILE_HANDLE "    z++;\n";
  print CFILE_HANDLE "  }\n";
  print CFILE_HANDLE "  errorTexts[index].text[i] = '\\0';\n";
  print CFILE_HANDLE "  errorTexts[errorTextCount].id = errorTextId;\n";
  print CFILE_HANDLE "  return index+1;\n";
  print CFILE_HANDLE "}\n";
  print CFILE_HANDLE "\n";
}
if ($hFileName ne "")
{
  open(HFILE_HANDLE,"> $hFileName");
  print HFILE_HANDLE "#ifndef __ERRORS__\n";
  print HFILE_HANDLE "#define __ERRORS__\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "#define ERROR(code,errno)       ((((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) |                                                                                   (((ERROR_ ## code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK))\n";
  print HFILE_HANDLE "#define ERRORX(code,errno,text) ((((errno) << $ERROR_ERRNO_SHIFT) & $ERROR_ERRNO_MASK) | ((_Errors_textToIndex(text) << $ERROR_TEXTINDEX_SHIFT) & $ERROR_TEXTINDEX_MASK) | (((ERROR_ ## code) << $ERROR_CODE_SHIFT) & $ERROR_CODE_MASK))\n";
  print HFILE_HANDLE "\n";
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
  if (($line =~ /^\s*$/) || ($line =~ /^\s*#/)) { next; }
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
