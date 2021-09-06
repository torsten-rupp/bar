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

my $TYPE_TABLE   = "TABLE";
my $TYPE_INDEX   = "INDEX";
my $TYPE_FTS     = "FTS";
my $TYPE_TRIGGER = "TRIGGER";

my %constants;

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

sub processFile($$)
{
  my $fileName = shift(@_);
  my $suffix   = shift(@_);

  my $commentFlag                   = 0;
  my $type                          = "";
  my @allDatabaseTableNames;
  my $allDatabaseDefinitions        = "";
  my $allDatabaseIndizesDefinitions = "";
  my $allDatabaseFTSDefinitions     = "";
  my @allDatabaseTriggerNames;
  my $allDatabaseTriggerDefinitions = "";
  my $databaseTableDefinitionName   = "";
  my $databaseTableDefinition       = "";
  my $databaseTriggerDefinitionName = "";
  my $id=0;

  # open file
  open(HANDLE, '<', $fileName);

  # read and process
  while ($line=<HANDLE>)
  {
    chop $line;
    $line =~ s/\/\/.*$//g;
    if ($line =~ /^\s*$/) { next; }

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
        $commentFlag = 1;
      }
      elsif ($line =~ /^\s*\#.*/)
      {
        # comment
      }
      elsif ($line =~ /^const\s+(\w+)\s*=\s+(\S*)\s*$/)
      {
        # constant
        my $name  = $1;
        my $value = $2;

        $constants{$name}=$value;

        if ($suffix eq "")
        {
          print "#define $PREFIX_CONST_NAME$name $value\n";
        }
      }
      elsif ($line =~ /^\s*CREATE\s+TABLE\s+.*?(\S+)\s*\(/)
      {
        # create table
        $type=$TYPE_TABLE;

        my $tableName = $1;

        # replace macros
        foreach $name (keys %constants)
        {
          $tableName =~ s/\$$name/$constants{$name}/g;
        }
        $tableName =~ s/"/\\"/g;

        push(@allDatabaseTableNames, $tableName);
        $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE TABLE IF NOT EXISTS $tableName (\\\n";
        $databaseTableDefinitionName=$tableName;
        $databaseTableDefinition="CREATE TABLE IF NOT EXISTS $tableName (\\\n";
      }
      elsif ($line =~ /^\s*CREATE\s+INDEX\s+ON\s+(\S+)\s+(.*?);$/)
      {
        # create anonymous index (single line)

        my $tableName  = $1;
        my $definition = $2;

        # replace macros
        foreach $name (keys %constants)
        {
          $tableName =~ s/\$$name/$constants{$name}/g;
        }
        $tableName =~ s/"/\\"/g;
        foreach $name (keys %constants)
        {
          $definition =~ s/\$$name/$constants{$name}/g;
        }
        $definition =~ s/"/\\"/g;

        # create index name
        my $indexName = "index$id"; $id++;

        if    ($suffix eq "SQLITE")
        {
          $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE INDEX IF NOT EXISTS $indexName ON $tableName $definition;\\\n";
          $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."CREATE INDEX IF NOT EXISTS $indexName ON $tableName $definition;\\\n";
        }
        elsif ($suffix eq "MYSQL")
        {
          $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE INDEX $indexName ON $table $definition;\\\n";
          $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."CREATE INDEX $indexName ON $table $definition;\\\n";
        }
      }
      elsif ($line =~ /^\s*CREATE\s+INDEX\s+ON\s+(\S+)\s+(.*?)$/)
      {
        # create anonymous index
        $type=$TYPE_INDEX;

        my $tableName  = $1;
        my $definition = $2;

        # replace macros
        foreach $name (keys %constants)
        {
          $tableName =~ s/^\$$name/$constants{$name}/g;
        }
        $table =~ s/"/\\"/g;
        foreach $name (keys %constants)
        {
          $definition =~ s/\$$name/$constants{$name}/g;
        }
        $definition =~ s/"/\\"/g;

        # create index name
        my $indexName = "index$id"; $id++;

        $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE INDEX IF NOT EXISTS $indexName ON $tableName $definition\\\n";
        $allDatabaseIndizesDefinitions=$allDatabaseIndizesDefinitions."CREATE INDEX IF NOT EXISTS $indexName ON $tableName $definition\\\n";
      }
      elsif ($line =~ /^\s*CREATE\s+VIRTUAL\s+TABLE\s+(FTS_\S+)\s+USING\s+(FTS.*)\s*\($/)
      {
        # create anonymous FTS
        $type=$TYPE_FTS;

        my $tableName = $1;
        my $type      = $2;

        # replace macros
        foreach $name (keys %constants)
        {
          $tableName =~ s/\$$name/$constants{$name}/g;
        }
        $table =~ s/"/\\"/g;
        foreach $name (keys %constants)
        {
          $type =~ s/\$$name/$constants{$name}/g;
        }
        $type =~ s/"/\\"/g;

        $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE VIRTUAL TABLE IF NOT EXISTS $tableName USING $type(\\\n";
        $allDatabaseFTSDefinitions=$allDatabaseFTSDefinitions."CREATE VIRTUAL TABLE IF NOT EXISTS $tableName USING $type(\\\n";
      }
      elsif ($line =~ /^\s*CREATE\s+TRIGGER\s+(BEFORE|AFTER)\s+(.*?)$/)
      {
        # create anonymous trigger
        $type=$TYPE_TRIGGER;

        my $definition = $2;

        # replace macros
        foreach $name (keys %constants)
        {
          $definition =~ s/\$$name/$constants{$name}/g;
        }
        $definition =~ s/"/\\"/g;

        # create trigger name
        my $triggerName = "trigger$id"; $id++;

        push(@allDatabaseTriggerNames, $triggerName);
        $allDatabaseDefinitions=$allDatabaseDefinitions."CREATE TRIGGER $triggerName $1 $definition\\\n";
        $allDatabaseTriggerDefinitions=$allDatabaseTriggerDefinitions."CREATE TRIGGER $triggerName $1 $2\\\n";
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

          if ($suffix ne "")
          {
            print "\n";
            print "#define INDEX_DEFINITION_".$suffix."_".uc($databaseTableDefinitionName)." \\\n\"\\\n";
            print $databaseTableDefinition;
            print "\"\n";
          }
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

        $type = "";
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

          if ($suffix ne "")
          {
            print "\n";
            print "#define INDEX_DEFINITION_".$suffix."_".uc($databaseTableDefinitionName)." \\\n\"\\\n";
            print $databaseTableDefinition;
            print "\"\n";
          }
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

        $type = "";
      }
      elsif ($line =~ /^\s*INSERT\s+OR\s+IGNORE\s+INTO\s+(\S+)\s+(.*?);$/)
      {
        # replace macros
        my $tableName = $1;
        my $values    = $2;
        foreach $name (keys %constants)
        {
          $tableName =~ s/\$$name/$constants{$name}/g;
          $values =~ s/\$$name/$constants{$name}/g;
        }
        $tableName =~ s/"/\\"/g;
        $values =~ s/"/\\"/g;

        $allDatabaseDefinitions=$allDatabaseDefinitions."INSERT IGNORE INTO $tableName $values;\\\n";
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

  # close file
  close(HANDLE);

  if ($suffix ne "")
  {
    # all definitions
    print "\n";
    print "const char* INDEX_DEFINITION_TABLE_NAMES_".$suffix."[] = \n";
    print "{\n";
    foreach (@allDatabaseTableNames)
    {
       print "  \"$_\",\n";
    }
    print "  NULL\n";
    print "};\n";
    print "\n";
    print "const char* INDEX_DEFINITION_TRIGGER_NAMES_".$suffix."[] = \n";
    print "{\n";
    foreach (@allDatabaseTriggerNames)
    {
       print "  \"$_\",\n";
    }
    print "  NULL\n";
    print "};\n";
    print "\n";
    print "#define INDEX_DEFINITION_".$suffix." \\\n\"\\\n";
    print $allDatabaseDefinitions;
    print "\"\n";
    print "\n";
    print "#define INDEX_INDIZES_DEFINITION_".$suffix." \\\n\"\\\n";
    print $allDatabaseIndizesDefinitions;
    print "\"\n";
    print "#define INDEX_FTS_INDIZES_DEFINITION_".$suffix." \\\n\"\\\n";
    print $allDatabaseFTSDefinitions;
    print "\"\n";
    print "#define INDEX_TRIGGERS_DEFINITION_".$suffix." \\\n\"\\\n";
    print $allDatabaseTriggerDefinitions;
    print "\"\n";
  }
}

# ------------------------------ main program  -------------------------------

# get options
GetOptions("common=s" => \$commonFileName,
           "sqlite=s" => \$sqliteFileName,
           "mysql=s"  => \$mysqlFileName,
           "help"     => \$help
          );

# help
if ($help == 1)
{
  print "Usage: $0 <options>\n";
  print "\n";
  print "Options: --common <file name> - comon definition file\n";
  print "         --sqlite <file name> - SqLite definition file\n";
  print "         --mysql <file name>  - MySQL definition file\n";
  print "         --help               - output this help\n";
  exit 0;
}

print "/* This file is auto-generated by archive_format.pl. Do NOT edit! */

#ifndef __INDEX_DEFINITION__
#define __INDEX_DEFINITION__

";

processFile($commonFileName,"");
processFile($sqliteFileName,"SQLITE");
processFile($mysqlFileName,"MYSQL");

print "\n";
print "#endif /* __INDEX_DEFINITION__ */\n";

exit 0;
# end of file
