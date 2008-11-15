#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/bar/errors.pl,v $
# $Revision: 1.1 $
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

my $FUNCTION_NAME = "getErrorText";
my $PREFIX        = "ERROR_";

my $cFileName,$hFileName;

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

sub writeHFile($)
{
  my $s=shift(@_);

  if ($hFileName ne "")
  {
    print HFILE_HANDLE $s;
  }
}

sub writeCFile($)
{
  my $s=shift(@_);

  if ($cFileName ne "")
  {
    print CFILE_HANDLE $s;
  }
}

sub writeHPrefix()
{
  print HFILE_HANDLE "typedef enum\n";
  print HFILE_HANDLE "{\n";
  print HFILE_HANDLE "  /*   0 */ ".$PREFIX."NONE,\n";
}

sub writeHPostfix()
{
  print HFILE_HANDLE "  ".$PREFIX."UNKNOWN\n";
  print HFILE_HANDLE "} Errors;\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "const char *getErrorText(Errors error);\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "#endif /* __ARCHIVE_FORMAT__ */\n";
}

sub writeCPrefix()
{
  print CFILE_HANDLE "#include \"errors.h\"\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "#define GET_ERROR_CODE(error) (((error) & 0x0000FFFF) >>  0)\n";
  print CFILE_HANDLE "#define GET_ERRNO(error) (((error) & 0xFFFF0000) >> 16)\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "#define ERRNO GET_ERRNO(error)\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "const char *$FUNCTION_NAME(Errors error)\n";
  print CFILE_HANDLE "{\n";
  print CFILE_HANDLE "  static char errorText[256];\n";
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

# ------------------------------ main program  -------------------------------

GetOptions("c=s" => \$cFileName,
           "h=s" => \$hFileName,
          );

if ($cFileName ne "")
{
  open(CFILE_HANDLE,"> $cFileName");
  print CFILE_HANDLE "#include <stdlib.h>\n";
  print CFILE_HANDLE "#include <string.h>\n";
  print CFILE_HANDLE "#include <errno.h>\n";
  print CFILE_HANDLE "\n";
}
if ($hFileName ne "")
{
  open(HFILE_HANDLE,"> $hFileName");
  print HFILE_HANDLE "#ifndef __ERRORS__\n";
  print HFILE_HANDLE "#define __ERRORS__\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "#define ERROR(code,errno) (((errno) << 16) | ERROR_ ## code)\n";
  print HFILE_HANDLE "\n";
  writeHPrefix();
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
    writeHFile("  $PREFIX$name,\n");
    if (!$writeCPrefixFlag) { writeCPrefix(); $writeCPrefixFlag = 1; }
    writeCFile("    case $PREFIX$name: return \"$text\";\n");
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
  elsif ($line =~ /^ERROR\s+(\w+)\s*$/)
  {
    # error <name>
    my $name=$1;
    writeHFile("  $PREFIX$name,\n");
    push(@names,$name);
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

exit 0;
# end of file
