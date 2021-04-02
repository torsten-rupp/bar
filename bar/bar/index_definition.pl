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

my $TYPE_TABLE="TABLE";
my $TYPE_INDEX="INDEX";
my $TYPE_FTS="FTS";
my $TYPE_TRIGGER="TRIGGER";

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

# ------------------------------ main program  -------------------------------

print "#ifndef __INDEX_DEFINITION__\n";
print "#define __INDEX_DEFINITION__\n";
print "\n";

my $commentFlag=0;
my %constants;
my $type="";
my $allDatabaseDefinitions = "";
my $allDatabaseIndizesDefinitions = "";
my $allDatabaseFTSDefinitions = "";
my $allDatabaseTriggerDefinitions = "";
my $databaseTableDefinitionName="";
my $databaseTableDefinition="";
my $databaseTriggerDefinitionName="";
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

        $allDatabaseDefinitions=$allDatabaseDefinitions."$1\\\n";
        if    ($type eq $TYPE_TABLE)
        {
          $databaseTableDefinition=$databaseTableDefinition."$1\\\n";
        }
        elsif ($type eq $TYPE_INDEX)
        {
          $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."$1\\\n";
        }
        elsif ($type eq $TYPE_FTS)
        {
          $allDatabaseFTSDefinitions=$allDatabaseFTSDefinitions."$1\\\n";
        }
        elsif ($type eq $TYPE_TRIGGER)
        {
          $allDatabaseTriggerDefinitions=$allDatabaseTriggerDefinitions."$1\\\n";
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

        $allDatabaseDefinitions=$allDatabaseDefinitions."$1\\\n";
        if    ($type eq $TYPE_TABLE)
        {
          $databaseTableDefinition=$databaseTableDefinition."$1\\\n";
        }
        elsif ($type eq $TYPE_INDEX)
        {
          $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."$1\\\n";
        }
        elsif ($type eq $TYPE_FTS)
        {
          $allDatabaseFTSDefinitions=$allDatabaseFTSDefinitions."$1\\\n";
        }
        elsif ($type eq $TYPE_TRIGGER)
        {
          $allDatabaseTriggerDefinitions=$allDatabaseTriggerDefinitions."$1\\\n";
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
    elsif ($line =~ /^\s*CREATE\s+TABLE\s+.*?(\S+)\s*\(/)
    {
      # create table
      $type=$TYPE_TABLE;

      # replace macros
      foreach $name (keys %constants)
      {
        $line =~ s/\$$name/$constants{$name}/g;
      }
      $line =~ s/"/\\"/g;

      $allDatabaseDefinitions=$allDatabaseDefinitions."$line\\\n";

      $databaseTableDefinitionName=$1;
      $databaseTableDefinition=$line;
    }
    elsif ($line =~ /^\s*CREATE\s+INDEX\s+ON\s+(\S+)\s+(.*?);$/)
    {
      # create anonymous index (single line)

      # replace macros
      my $table=$1;
      foreach $name (keys %constants)
      {
        $table =~ s/\$$name/$constants{$name}/g;
      }
      $table =~ s/"/\\"/g;
      my $definition=$2;
      foreach $name (keys %constants)
      {
        $definition =~ s/\$$name/$constants{$name}/g;
      }
      $definition =~ s/"/\\"/g;

      # create index name
      my $index="index$id"; $id++;

      $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE INDEX IF NOT EXISTS $index ON $table $definition;\\\n";
      $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."CREATE INDEX IF NOT EXISTS $index ON $table $definition;\\\n";
    }
    elsif ($line =~ /^\s*CREATE\s+INDEX\s+ON\s+(\S+)\s+(.*?)$/)
    {
      # create anonymous index
      $type=$TYPE_INDEX;

      # replace macros
      my $table=$1;
      foreach $name (keys %constants)
      {
        $table =~ s/^\$$name/$constants{$name}/g;
      }
      $table =~ s/"/\\"/g;
      my $definition=$2;
      foreach $name (keys %constants)
      {
        $definition =~ s/\$$name/$constants{$name}/g;
      }
      $definition =~ s/"/\\"/g;

      # create index name
      my $index="index$id"; $id++;

      $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE INDEX IF NOT EXISTS $index ON $table $definition\\\n";
      $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."CREATE INDEX IF NOT EXISTS $index ON $table $definition\\\n";
    }
    elsif ($line =~ /^\s*CREATE\s+VIRTUAL\s+TABLE\s+(FTS_\S+)\s+USING\s+(FTS.*)\s*\($/)
    {
      # create anonymous FTS
      $type=$TYPE_FTS;

      # replace macros
      my $table=$1;
      foreach $name (keys %constants)
      {
        $table =~ s/\$$name/$constants{$name}/g;
      }
      $table =~ s/"/\\"/g;
      my $type=$2;
      foreach $name (keys %constants)
      {
        $type =~ s/\$$name/$constants{$name}/g;
      }
      $type =~ s/"/\\"/g;

      $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE VIRTUAL TABLE IF NOT EXISTS $table USING $type(\\\n";
      $allDatabaseFTSDefinitions=$allDatabaseFTSDefinitions."CREATE VIRTUAL TABLE IF NOT EXISTS $table USING $type(\\\n";
    }
    elsif ($line =~ /^\s*CREATE\s+TRIGGER\s+(BEFORE|AFTER)\s+(.*?)$/)
    {
      # create anonymous trigger
      $type=$TYPE_TRIGGER;

      # replace macros
      my $definition=$2;
      foreach $name (keys %constants)
      {
        $definition =~ s/\$$name/$constants{$name}/g;
      }
      $definition =~ s/"/\\"/g;

      # create trigger name
      my $trigger="trigger$id"; $id++;

      $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE TRIGGER $trigger $1 $definition\\\n";
      $allDatabaseTriggerDefinitions=$allDatabaseTriggerDefinitions."CREATE TRIGGER $trigger $1 $2\\\n";
    }
    elsif ($line =~ /^\s*END\s*$/)
    {
      # end trigger

      # replace macros
      foreach $name (keys %constants)
      {
        $line =~ s/\$$name/$constants{$name}/g;
      }
      $line =~ s/"/\\"/g;

      $allDatabaseDefinitions=$allDatabaseDefinitions."$line\\\n";
      if    ($type eq $TYPE_TABLE)
      {
        $databaseTableDefinition=$databaseTableDefinition."$line\\\n";

        print "\n";
        print "#define INDEX_DEFINITION_".uc($databaseTableDefinitionName)." \\\n\"\\\n";
        print $databaseTableDefinition;
        print "\"\n";
      }
      elsif ($type eq $TYPE_INDEX)
      {
        $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."$line\\\n";
      }
      elsif ($type eq $TYPE_FTS)
      {
        $allDatabaseFTSDefinitions=$allDatabaseFTSDefinitions."$line\\\n";
      }
      elsif ($type eq $TYPE_TRIGGER)
      {
        $allDatabaseTriggerDefinitions=$allDatabaseTriggerDefinitions."$line\\\n";
      }

      $type="";
    }
    elsif ((($type eq $TYPE_TABLE) || ($type eq $TYPE_INDEX)) && ($line =~ /\);/))
    {
      # end table/index

      # replace macros
      foreach $name (keys %constants)
      {
        $line =~ s/\$$name/$constants{$name}/g;
      }
      $line =~ s/"/\\"/g;

      $allDatabaseDefinitions=$allDatabaseDefinitions."$line\\\n";
      if    ($type eq $TYPE_TABLE)
      {
        $databaseTableDefinition=$databaseTableDefinition."$line\\\n";

        print "\n";
        print "#define INDEX_DEFINITION_".uc($databaseTableDefinitionName)." \\\n\"\\\n";
        print $databaseTableDefinition;
        print "\"\n";
      }
      elsif ($type eq $TYPE_INDEX)
      {
        $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."$line\\\n";
      }
      elsif ($type eq $TYPE_FTS)
      {
        $allDatabaseFTSDefinitions=$allDatabaseFTSDefinitions."$line\\\n";
      }
      elsif ($type eq $TYPE_TRIGGER)
      {
        $allDatabaseTriggerDefinitions=$allDatabaseTriggerDefinitions."$line\\\n";
      }

      $type="";
    }
    else
    {
      # replace macros
      foreach $name (keys %constants)
      {
        $line =~ s/\$$name/$constants{$name}/g;
      }
      $line =~ s/"/\\"/g;

      $allDatabaseDefinitions=$allDatabaseDefinitions."$line\\\n";
      if    ($type eq $TYPE_TABLE)
      {
        $databaseTableDefinition=$databaseTableDefinition."$line\\\n";
      }
      elsif ($type eq $TYPE_INDEX)
      {
        $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."$line\\\n";
      }
      elsif ($type eq $TYPE_FTS)
      {
        $allDatabaseFTSDefinitions=$allDatabaseFTSDefinitions."$line\\\n";
      }
      elsif ($type eq $TYPE_TRIGGER)
      {
        $allDatabaseTriggerDefinitions=$allDatabaseTriggerDefinitions."$line\\\n";
      }
    }
  }
}

# all definitions
print "\n";
print "#define INDEX_DEFINITION \\\n\"\\\n";
print $allDatabaseDefinitions;
print "\"\n";
print "\n";
print "#define INDEX_INDIZES_DEFINITION \\\n\"\\\n";
print $allDatabaseIndizesDefinitions;
print "\"\n";
print "#define INDEX_FTS_INDIZES_DEFINITION \\\n\"\\\n";
print $allDatabaseFTSDefinitions;
print "\"\n";
print "#define INDEX_TRIGGERS_DEFINITION \\\n\"\\\n";
print $allDatabaseTriggerDefinitions;
print "\"\n";

print "\n";
print "#endif /* __INDEX_DEFINITION__ */\n";

exit 0;
# end of file
