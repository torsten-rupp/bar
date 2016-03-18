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
my $id=0;
while ($line=<STDIN>)
{
  chop $line;
  $line =~ s/\/\/.*$//g;
  if ($line =~ /^\s*$/) { next; }

  # get type of statement
  my $statementType;
  if    ($line =~ /^\s*CREATE\s+TABLE/  ) { $statementType="table"; }
  elsif ($line =~ /^\s*CREATE\s+INDEX/  ) { $statementType="index"; }
  elsif ($line =~ /^\s*CREATE\s+TRIGGER/) { $statementType="trigger"; }
  else                                    { $statementType=""; }

  if ($commentFlag)
  {
    if ($line =~ /\*\/\s*(.*)/)
    {
      # end comment
      $commentFlag=0;

      if ($1 ne "")
      {
        # replace macros
        $1 =~ s/\$version/$version/g;
        $1 =~ s/"/\\"/g;

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
      if ($1 ne "")
      {
        # replace macros
        $1 =~ s/\$version/$version/g;
        $1 =~ s/"/\\"/g;

        $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$1\\\n";
        if ($databaseTableDefinitionName ne "")
        {
          $databaseTableDefinition=$databaseTableDefinition."$1\\\n";
        }
      }

      # start comment
      $commentFlag=1;
    }
    elsif ($line =~ /^\s*\#.*/)
    {
      # comment
    }
    elsif ($line =~ /\s*VERSION\s*=\s*(\d+)\s*;\s*$/)
    {
      # VERSION=...
      $version=$1;

      print "#define INDEX_VERSION ".$version."\n\n";
    }
    elsif ($line =~ /\s*CREATE\s+TABLE\s+.*?(\S+)\s*\(/)
    {
      # create table

      # replace macros
      $line =~ s/\$version/$version/g;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$line\\\n";
      $databaseTableDefinitionName=$1;
      $databaseTableDefinition=$line;
    }
#CREATE INDEX IF NOT EXISTS filesIndex1 ON files (storageId,name);
    elsif ($line =~ /\s*CREATE\s+INDEX\s+ON\s+(\S+)\s+(.*?)$/)
    {
      # create anonymous index

      # replace macros
      $1 =~ s/\$version/$version/g;
      $1 =~ s/"/\\"/g;
      $2 =~ s/\$version/$version/g;
      $2 =~ s/"/\\"/g;

      # create index name
      my $index="index$id"; $id++;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."CREATE INDEX $index ON $1 $2\\\n";
      if ($databaseTableDefinitionName ne "")
      {
        $databaseTableDefinition=$databaseTableDefinition."CREATE INDEX $index ON $1 $2\\\n";
      }
    }
#CREATE TRIGGER AFTER INSERT ON files
    elsif ($line =~ /\s*CREATE\s+TRIGGER\s+(BEFORE|AFTER)\s+(.*?)$/)
    {
      # create anonymous trigger

      # replace macros
      $2 =~ s/\$version/$version/g;
      $2 =~ s/"/\\"/g;

      # create trigger name
      my $trigger="trigger$id"; $id++;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."CREATE TRIGGER $trigger $1 $2\\\n";
      if ($databaseTableDefinitionName ne "")
      {
        $databaseTableDefinition=$databaseTableDefinition."CREATE TRIGGER $trigger $1 $2\\\n";
      }
    }
    elsif ($line =~ /\);/)
    {
      # end table/index

      # replace macros
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
      # replace macros
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
