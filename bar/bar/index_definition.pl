#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Revision$
# $Date$
# $Author$
# Contents: create header file definition from database definition
# Systems: all
#
# ----------------------------------------------------------------------------

# ---------------------------- additional packages ---------------------------
use English;
use POSIX;
use Getopt::Std;
use Getopt::Long;

# ---------------------------- constants/variables ---------------------------

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

# ------------------------------ main program  -------------------------------

my $commentFlag=0;
my $version=0;
my $allDatabaseTableDefinitions = "";
my $databaseTableDefinitionName="";
my $databaseTableDefinition="";
while ($line=<STDIN>)
{
  chop $line;
  if ($line =~ /^\s*\/\// || $line =~ /^\s*$/) { next; }

  if ($commentFlag)
  {
    if ($line =~ /\*\/\s*(.*)/)
    {
      $commentFlag=0;

      if ($1 ne "")
      {
        $1 =~ s/\$version/$version/g;

        $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$1\\\n";
        if ($databaseTableDefinitionName ne "")
        {
          $databaseTableDefinition=$databaseTableDefinition."$1\\\n";
        }
      }
    }
  }
  else
  {
    if    ($line =~ /(.*)\s*\/\*/)
    {
      $commentFlag=1;

      if ($1 ne "")
      {
        $1 =~ s/\$version/$version/g;

        $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$1\\\n";
        if ($databaseTableDefinitionName ne "")
        {
          $databaseTableDefinition=$databaseTableDefinition."$1\\\n";
        }
      }
    }
    elsif ($line =~ /^\s*\#.*/)
    {
    }
    elsif ($line =~ /\s*VERSION\s*=\s*(\d+)\s*;\s*$/)
    {
      $version=$1;

      print "#define INDEX_VERSION ".$version."\n\n";
    }
    elsif ($line =~ /\s*CREATE\s+TABLE\s+.*?(\S+)\s*\(/)
    {
      $line =~ s/\$version/$version/g;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$line\\\n";
      $databaseTableDefinitionName=$1;
      $databaseTableDefinition=$line;
    }
    elsif ($line =~ /\);/)
    {
      $line =~ s/\$version/$version/g;
      $line =~ s/"/\\"/g;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$line\\\n";
      if ($databaseTableDefinitionName ne "")
      {
        $databaseTableDefinition=$databaseTableDefinition."$line\\\n";

        print "#define INDEX_TABLE_DEFINITION_".uc($databaseTableDefinitionName)." \\\n\"\\\n";
        print $databaseTableDefinition;
        print "\"\n";
        print "\n";

        $databaseTableDefinitionName="";
      }
    }
    else
    {
      $line =~ s/\$version/$version/g;
      $line =~ s/"/\\"/g;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$line\\\n";
      if ($databaseTableDefinitionName ne "")
      {
        $databaseTableDefinition=$databaseTableDefinition."$line\\\n";
      }
    }
  }
}

# all definitions
print "#define INDEX_TABLE_DEFINITION \\\n\"\\\n";
print $allDatabaseTableDefinitions;
print "\"\n";

exit 0;
# end of file
