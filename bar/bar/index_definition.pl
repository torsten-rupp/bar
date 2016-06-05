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

my $PREFIX_CONST_NAME = "INDEX_CONST_";

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

# ------------------------------ main program  -------------------------------

print "#ifndef __INDEX_DEFINITION__\n";
print "#define __INDEX_DEFINITION__\n";
print "\n";

my $commentFlag=0;
my %constants;
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
        foreach $name (keys %constants)
        {
          $1 =~ s/\$$name/$constants{$name}/g;
        }
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
        foreach $name (keys %constants)
        {
          $1 =~ s/\$$name/$constants{$name}/g;
        }
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
    elsif ($line =~ /^const\s+(\w+)\s*=\s+(\S*)\s*$/)
    {
      # constant
      my $name =$1;
      my $value=$2;

      $constants{$name}=$value;

      print "#define $PREFIX_CONST_NAME$name $value\n";
    }
    elsif ($line =~ /\s*CREATE\s+TABLE\s+.*?(\S+)\s*\(/)
    {
      # create table

      # replace macros
      foreach $name (keys %constants)
      {
        $line =~ s/\$$name/$constants{$name}/g;
      }
      $line =~ s/"/\\"/g;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$line\\\n";
      $databaseTableDefinitionName=$1;
      $databaseTableDefinition=$line;
    }
    elsif ($line =~ /\s*CREATE\s+INDEX\s+ON\s+(\S+)\s+(.*?)$/)
    {
      # create anonymous index

      # replace macros
      my $table=$1;
      foreach $name (keys %constants)
      {
        $table =~ s/\$$name/$constants{$name}/g;
      }
      $table =~ s/"/\\"/g;
      my $definition=$1;
      foreach $name (keys %constants)
      {
        $definition =~ s/\$$name/$constants{$name}/g;
      }
      $definition =~ s/"/\\"/g;

      # create index name
      my $index="index$id"; $id++;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."CREATE INDEX $index ON $table $definition\\\n";
      if ($databaseTableDefinitionName ne "")
      {
        $databaseTableDefinition=$databaseTableDefinition."CREATE INDEX $index ON $table $definition\\\n";
      }
    }
    elsif ($line =~ /\s*CREATE\s+TRIGGER\s+(BEFORE|AFTER)\s+(.*?)$/)
    {
      # create anonymous trigger

      # replace macros
      my $definition=$1;
      foreach $name (keys %constants)
      {
        $definition =~ s/\$$name/$constants{$name}/g;
      }
      $definition =~ s/"/\\"/g;

      # create trigger name
      my $trigger="trigger$id"; $id++;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."CREATE TRIGGER $trigger $1 $definition\\\n";
      if ($databaseTableDefinitionName ne "")
      {
        $databaseTableDefinition=$databaseTableDefinition."CREATE TRIGGER $trigger $1 $2\\\n";
      }
    }
    elsif ($line =~ /\);/)
    {
      # end table/index

      # replace macros
      foreach $name (keys %constants)
      {
        $line =~ s/\$$name/$constants{$name}/g;
      }
      $line =~ s/"/\\"/g;

      $allDatabaseTableDefinitions=$allDatabaseTableDefinitions."$line\\\n";
      if ($databaseTableDefinitionName ne "")
      {
        $databaseTableDefinition=$databaseTableDefinition."$line\\\n";

        print "\n";
        print "#define INDEX_DEFINITION_".uc($databaseTableDefinitionName)." \\\n\"\\\n";
        print $databaseTableDefinition;
        print "\"\n";

        $databaseTableDefinitionName="";
      }
    }
    else
    {
      # replace macros
      foreach $name (keys %constants)
      {
        $line =~ s/\$$name/$constants{$name}/g;
      }
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
print "\n";
print "#define INDEX_DEFINITION \\\n\"\\\n";
print $allDatabaseTableDefinitions;
print "\"\n";

print "\n";
print "#endif /* __INDEX_DEFINITION__ */\n";

exit 0;
# end of file
