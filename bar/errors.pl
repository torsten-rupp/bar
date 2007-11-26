#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/errors.pl,v $
# $Revision: 1.3 $
# $Author: torsten $
# Contents: create header/c file definition from errors definition
# Systems: all
#
# ----------------------------------------------------------------------------
# Exported Functions
#
# ----------------------------------------------------------------------------
# Function                       Purpose
# ----------------------------------------------------------------------------
#
# ----------------------------------------------------------------------------

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
  print CFILE_HANDLE "#include \"errors.h\"\n";
  print CFILE_HANDLE "\n";

  print CFILE_HANDLE "const char *$FUNCTION_NAME(Errors error)\n";
  print CFILE_HANDLE "{\n";
  print CFILE_HANDLE "  static char errorText[256];\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "  strcpy(errorText,\"unknown\");\n";
  print CFILE_HANDLE "  switch (error)\n";
  print CFILE_HANDLE "  {\n";
}
if ($hFileName ne "")
{
  open(HFILE_HANDLE,"> $hFileName");
  print HFILE_HANDLE "#ifndef __ERRORS__\n";
  print HFILE_HANDLE "#define __ERRORS__\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "typedef enum\n";
  print HFILE_HANDLE "{\n";
  print HFILE_HANDLE "  /*   0 */ ".$PREFIX."NONE,\n";
}

my @names;
my $defaultText;
my $line;
my $lineNb=0;
while ($line=<STDIN>)
{
  chop $line;
  $lineNb++;
  if ($line =~ /^\s*#/) { next; }
#print "$line\n";

  if    ($line =~ /^ERROR\s+(\w+)\s+"(.*)"\s*$/)
  {
    # error <name> <text>
    writeHFile("  $PREFIX$1,\n");
    writeCFile("    case $PREFIX$1: return \"$2\";\n");
  }
  elsif ($line =~ /^NONE\s+"(.*)"\s*$/)
  {
    # none <text>
    writeCFile("    case ".$PREFIX."NONE: return \"$1\";\n");
  }
  elsif ($line =~ /^DEFAULT\s+"(.*)"\s*$/)
  {
    $defaultText=$1;
  }
  elsif ($line =~ /^ERROR\s+(\w+)\s*$/)
  {
    # error <name>
    writeHFile("  $PREFIX$1,\n");
    push(@names,$1);
  }
  elsif ($line =~ /^\s*$/)
  {
    # end of code
    @names=();
  }
  else
  {
    # code
    if (scalar(@names) <= 0)
    {
      print STDERR "Unknown data '$line' in line $lineNb\n";
      exit 1;
    }

    foreach my $z (@names)
    {
      writeCFile("    case $PREFIX$z:\n");
    }
    writeCFile("    $line\n");
    while ($line=<STDIN>)
    {
      chop $line;
      $lineNb++;
      if ($line =~ /^\s*$/) { last; }

      writeCFile("    $line\n");
    }
    writeCFile("      break;\n");
  }
}

if ($cFileName ne "")
{
  if ($defaultText ne "")
  {
    writeCFile("    default: return \"$defaultText\";\n");
  }
  print CFILE_HANDLE "  }\n";
  print CFILE_HANDLE "\n";
  print CFILE_HANDLE "  return errorText;\n";
  print CFILE_HANDLE "}\n";
  close(CFILE_HANDLE);
}
if ($hFileName ne "")
{
  print HFILE_HANDLE "  ".$PREFIX."UNKNOWN\n";
  print HFILE_HANDLE "} Errors;\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "const char *getErrorText(Errors error);\n";
  print HFILE_HANDLE "\n";
  print HFILE_HANDLE "#endif /* __ARCHIVE_FORMAT__ */\n";
  close(HFILE_HANDLE);
}

exit 0;
# end of file
