#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Revision$
# $Date$
# $Author$
# Contents: create header/implemenetation file definition from database
#           definitions
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

my $TYPE_TABLE    = "TABLE";
my $TYPE_INDEX    = "INDEX";
my $TYPE_FTS      = "FTS";
my $TYPE_TRIGGER  = "TRIGGER";
my $TYPE_FUNCTION = "FUNCTION";
my $TYPE_VIEW     = "VIEW";


my %constants;
my $id=0;

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

#***********************************************************************
# Name   : expandMacros
# Purpose: expand macros
# Input  : string string
# Output : -
# Return : expanded string
# Notes  : -
#***********************************************************************

sub expandMacros($)
{
  my $string = shift(@_);

  foreach $name (keys %constants)
  {
    $string =~ s/\$\{$name\}/$constants{$name}/g;
  }
  $string =~ s/"/\\"/g;

  return $string;
}

#***********************************************************************
# Name   : getName
# Purpose: generate name
# Input  : prefix prefix
# Output : -
# Return : name
# Notes  : -
#***********************************************************************

sub getName($)
{
  my $prefix = shift(@_);

  my $name = "bar_".$prefix."$id";
  $id++;

  return $name;
}

#***********************************************************************
# Name   : append
# Purpose: append definition line
# Input  : definition definition
#          line       line
# Output : -
# Return : new definition
# Notes  : -
#***********************************************************************

sub append($$)
{
  my $line   = shift(@_);
  my $string = shift(@_);

  if ($line ne "")
  {
    $line = $line." \\\n";
  }
  return $line.$string;
}

#***********************************************************************
# Name   : processFile
# Purpose: process file
# Input  : fileName file name
#          suffix   database suffix
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

sub processFile($$)
{
  my $fileName = shift(@_);
  my $suffix   = shift(@_);

  my $commentFlag                   = 0;
  my $type                          = "";
  my @definitions;
  my @tableNames;
  my @ftsTableNames;
  my @indexNames;
  my @triggerNames;
  my @functionNames;
  my @viewNames;

  # open file
  open(HANDLE, '<', $fileName);

  # read and process
  my $definition         = "";
  my $tableName          = "";
  my $ftsType            = "";
  my $triggerType        = "";
  my $triggerOperation   = "";
  my $triggerOperationOf = "";
  my $indexName          = "";
  my $triggerName        = "";
  my $functionName       = "";
  my $viewName           = "";
  while ($line=<HANDLE>)
  {
    chop $line;

    # remove //-comment at line end
    $line =~ s/\s*\/\/.*$//g;

    # skip empty lines
    if ($line =~ /^\s*$/) { next; }

    if ($commentFlag)
    {
      if ($line =~ /\*\/\s*(.*)/)
      {
        # end of multi-line comment
        $commentFlag=0;

        if ($1 ne "")
        {
          $1 = expandMacros($1);

          if    ($type eq $TYPE_TABLE)
          {
          }
          elsif ($type eq $TYPE_INDEX)
          {
          }
          elsif ($type eq $TYPE_FTS)
          {
          }
          elsif ($type eq $TYPE_TRIGGER)
          {
          }
        }
      }
    }
    else
    {
      if    ($line =~ /(.*)\s*\/\*/)
      {
        # multi-line comment
        if ($1 ne "")
        {
          $1 = expandMacros($1);

          if    ($type eq $TYPE_TABLE)
          {
          }
          elsif ($type eq $TYPE_INDEX)
          {
          }
          elsif ($type eq $TYPE_FTS)
          {
          }
          elsif ($type eq $TYPE_TRIGGER)
          {
          }
        }

        # start multi-line comment
        $commentFlag = 1;
      }
      elsif ($line =~ /^\s*\#.*/)
      {
        # comment: skipped
      }
      elsif ($line =~ /^const\s+(\w+)\s*=\s+(\S*)\s*$/)
      {
        # constant
        my $name  = expandMacros($1);
        my $value = expandMacros($2);

        $constants{$name} = $value;
      }
      elsif ($line =~ /^\s*CREATE\s+TABLE\s+.*?(\S+)\s*\(/)
      {
        # create table (multi-line)
        $type = $TYPE_TABLE;

        $tableName = expandMacros($1);

        $definition = "";
      }
      elsif ($line =~ /^\s*CREATE\s+INDEX\s+ON\s+(\S+)\s+\((.*?)\);$/)
      {
        # create anonymous index (single line)
        $tableName = expandMacros($1);
        $columns   = expandMacros($2);

        $indexName = getName("index");

        if    ($suffix eq "SQLITE")
        {
          push(@indexNames, $indexName);
          push(@definitions, "INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE INDEX IF NOT EXISTS $indexName ON $tableName ($columns)\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)." = INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "MARIADB")
        {
          push(@indexNames, $indexName);
          push(@definitions, "INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE INDEX $indexName ON $tableName ($columns)\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)." = INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "POSTGRESQL")
        {
          push(@indexNames, $indexName);
          push(@definitions, "INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE INDEX IF NOT EXISTS $indexName ON $tableName ($columns)\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)." = INDEX_DEFINITION_INDEX_".$suffix."_".uc($indexName)."_;\n";
          print CFILE_HANDLE "\n";
        }

        $type       = "";
        $definition = "";
      }
      elsif ($line =~ /^\s*CREATE\s+VIRTUAL\s+TABLE\s+(FTS_\S+)\s+USING\s+(FTS.*)\s*\($/)
      {
        # create anonymous FTS (multi-line)
        $type = $TYPE_FTS;

        $tableName = expandMacros($1);
        $ftsType   = expandMacros($2);

        $definition = "";
      }
      elsif ($line =~ /^\s*CREATE\s+TRIGGER\s+(BEFORE|AFTER)\s+(UPDATE)\s+OF\s+(\S+)\s+ON\s+(.*?)$/)
      {
        # create anonymous trigger (multi-line)
        $type = $TYPE_TRIGGER;

        $triggerType        = expandMacros($1);
        $triggerOperation   = expandMacros($2);
        $triggerOperationOf = expandMacros($3);
        $tableName          = expandMacros($4);
        $triggerName        = getName("trigger");

        $definition = "";
      }
      elsif ($line =~ /^\s*CREATE\s+TRIGGER\s+(BEFORE|AFTER)\s+(UPDATE)\s+ON\s+(.*?)$/)
      {
        # create anonymous trigger (multi-line)
        $type = $TYPE_TRIGGER;

        $triggerType        = expandMacros($1);
        $triggerOperation   = expandMacros($2);
        $triggerOperationOf = "";
        $tableName          = expandMacros($3);
        $triggerName        = getName("trigger");

        $definition = "";
      }
      elsif ($line =~ /^\s*CREATE\s+TRIGGER\s+(BEFORE|AFTER)\s+(INSERT|DELETE)\s+ON\s+(.*?)$/)
      {
        # create anonymous trigger (multi-line)
        $type = $TYPE_TRIGGER;

        $triggerType      = expandMacros($1);
        $triggerOperation = expandMacros($2);
        $tableName        = expandMacros($3);
        $triggerName      = getName("trigger");

        $definition = "";
      }
      elsif ($line =~ /^\s*CREATE\s+VIEW\s+(.*?)\s*\($/)
      {
        # create view (multi-line)
        $type = $TYPE_VIEW;

        $viewName = expandMacros($1);

        $definition = "";
      }
      elsif ($line =~ /^\s*END;\s*$/)
      {
        # end trigger
#        $definition = append($definition,"END");

        if    ($type eq $TYPE_TABLE)
        {
          die "Invalid type 'table'";
        }
        elsif ($type eq $TYPE_INDEX)
        {
          die "Invalid type 'index'";
        }
        elsif ($type eq $TYPE_FTS)
        {
          die "Invalid type 'fts'";
        }
        elsif ($type eq $TYPE_TRIGGER)
        {
          if    ($suffix eq "SQLITE")
          {
            push(@triggerNames, $triggerName);
            push(@definitions, "INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE TRIGGER IF NOT EXISTS $triggerName $triggerOperation";
            if ($triggerOperationOf ne "")
            {
              print CFILE_HANDLE " OF $triggerOperationOf";
            }
            print CFILE_HANDLE " ON $tableName \\\n";
            print CFILE_HANDLE "BEGIN\\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "END;\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)." = INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)."_;\n";
          }
          elsif ($suffix eq "MARIADB")
          {
            push(@triggerNames, $triggerName);
            push(@definitions, "INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE TRIGGER $triggerName $triggerType $triggerOperation ON $tableName \\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)." = INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)."_;\n";
          }
          elsif ($suffix eq "POSTGRESQL")
          {
            push(@triggerNames, $triggerName);
            push(@definitions, "INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE TRIGGER $triggerName $triggerType $triggerOperation ON $tableName \\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)." = INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($triggerName)."_;\n";
          }
        }
        elsif ($type eq $TYPE_VIEW)
        {
          die "Invald type 'view'";
        }
        else
        {
          die "Unexpected 'END' ";
        }

        $type       = "";
        $definition = "";
      }
      elsif (   ($line =~ /\)(.*);\s*$/)
             && (   ($type eq $TYPE_TABLE)
                 || ($type eq $TYPE_FTS)
                 || ($type eq $TYPE_INDEX)
                )
            )
      {
        # end table/FTS/trigger/view
        $definition = append($definition,")".expandMacros($1));

        if    ($type eq $TYPE_TABLE)
        {
          if    ($suffix eq "SQLITE")
          {
            push(@tableNames, $tableName);
            push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE TABLE IF NOT EXISTS $tableName (\\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
            print CFILE_HANDLE "\n";
          }
          elsif ($suffix eq "MARIADB")
          {
            push(@tableNames, $tableName);
            push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE TABLE IF NOT EXISTS $tableName (\\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
            print CFILE_HANDLE "\n";
          }
          elsif ($suffix eq "POSTGRESQL")
          {
            push(@tableNames, $tableName);
            push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE TABLE IF NOT EXISTS $tableName (\\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
            print CFILE_HANDLE "\n";
          }
        }
        elsif ($type eq $TYPE_INDEX)
        {
          die "Invalid type 'index'";
        }
        elsif ($type eq $TYPE_FTS)
        {
          if    ($suffix eq "SQLITE")
          {
            push(@ftsTableNames, $tableName);
            push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE VIRTUAL TABLE IF NOT EXISTS $tableName USING $ftsType(\\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
            print CFILE_HANDLE "\n";
          }
          elsif ($suffix eq "MARIADB")
          {
            push(@ftsTableNames, $tableName);
            push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE VIEW IF NOT EXISTS $tableName(\\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
            print CFILE_HANDLE "\n";
          }
          elsif ($suffix eq "POSTGRESQL")
          {
            push(@ftsTableNames, $tableName);
            push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

            print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

            print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
            print CFILE_HANDLE "\"\\\n";
            print CFILE_HANDLE "CREATE OR REPLACE VIEW $tableName\\\n";
            print CFILE_HANDLE $definition."\\\n";
            print CFILE_HANDLE "\"\n";
            print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
            print CFILE_HANDLE "\n";
          }
        }
        elsif ($type eq $TYPE_TRIGGER)
        {
          die "Invalid type 'trigger'";
        }
        elsif ($type eq $TYPE_VIEW)
        {
          push(@definitions, "INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE VIEW $viewName AS \\\n";
          print CFILE_HANDLE $definition."\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName)." = INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        else
        {
          die "Unexpected ')' ";
        }

        $type       = "";
        $definition = "";
      }
      elsif (   ($line =~ /\)(.*);\s*$/)
             && ($type eq $TYPE_TABLE)
            )
      {
        # end table
        $definition = append($definition,")".expandMacros($1));

        if    ($suffix eq "SQLITE")
        {
          push(@tableNames, $tableName);
          push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE TABLE IF NOT EXISTS $tableName (\\\n";
          print CFILE_HANDLE $definition."\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "MARIADB")
        {
          push(@tableNames, $tableName);
          push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE TABLE IF NOT EXISTS $tableName (\\\n";
          print CFILE_HANDLE $definition."\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "POSTGRESQL")
        {
          push(@tableNames, $tableName);
          push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE TABLE IF NOT EXISTS $tableName (\\\n";
          print CFILE_HANDLE $definition."\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
          print CFILE_HANDLE "\n";
        }

        $type       = "";
        $definition = "";
      }
      elsif (   ($line =~ /\)(.*);\s*$/)
             && ($type eq $TYPE_FTS)
            )
      {
        # end FTS
        $definition = append($definition,")".expandMacros($1));

        if    ($suffix eq "SQLITE")
        {
          push(@ftsTableNames, $tableName);
          push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE VIRTUAL TABLE IF NOT EXISTS $tableName USING $ftsType(\\\n";
          print CFLILE_HANDLE $definition."\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "MARIADB")
        {
          push(@ftsTableNames, $tableName);
          push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE VIEW IF NOT EXISTS $tableName(\\\n";
          print CFILE_HANDLE $definition."\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "POSTGRESQL")
        {
          push(@ftsTableNames, $tableName);
          push(@definitions, "INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName));

          print HFILE_HANDLE "extern const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName).";\n";

          print CFILE_HANDLE "#define INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "CREATE VIEW IF NOT EXISTS $tableName(\\\n";
          print CFILE_HANDLE $definition."\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)." = INDEX_DEFINITION_TABLE_".$suffix."_".uc($tableName)."_;\n";
          print CFILE_HANDLE "\n";
        }

        $type       = "";
        $definition = "";
      }
      elsif (   ($line =~ /\)(.*);\s*$/)
             && ($type eq $TYPE_VIEW)
            )
      {
        # end view
        push(@definitions, "INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName));

        print HFILE_HANDLE "extern const char *INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName).";\n";

        print CFILE_HANDLE "#define INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName)."_ \\\n";
        print CFILE_HANDLE "\"\\\n";
        print CFILE_HANDLE "CREATE OR REPLACE VIEW $viewName AS \\\n";
        print CFILE_HANDLE $definition."\\\n";
        print CFILE_HANDLE "\"\n";
        print CFILE_HANDLE "const char *INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName)." = INDEX_DEFINITION_VIEW_".$suffix."_".uc($viewName)."_;\n";
        print CFILE_HANDLE "\n";

        $type       = "";
        $definition = "";
      }
      elsif ($line =~ /^\s*INSERT\s+INTO\s+(\S+)\s+(.*?);$/)
      {
        # insert statement
        my $tableName = expandMacros($1);
        my $values    = expandMacros($2);

        my $insertName = getName("insert");

        push(@definitions, "INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName));

        if    ($suffix eq "SQLITE")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "INSERT INTO $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "MARIADB")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "INSERT INTO $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "POSTGRESQL")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "INSERT INTO $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }

        $definition = "";
      }
      elsif ($line =~ /^\s*INSERT\s+OR\s+IGNORE\s+INTO\s+(\S+)\s+(.*?);$/)
      {
        # insert statement
        my $tableName = expandMacros($1);
        my $values    = expandMacros($2);

        my $insertName = getName("insert");

        push(@definitions, "INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName));

        if    ($suffix eq "SQLITE")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "INSERT OR IGNORE INTO $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "MARIADB")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "INSERT IGNORE INTO $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "POSTGRESQL")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "INSERT IGNORE INTO $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }

        $definition = "";
      }
      elsif ($line =~ /^\s*UPDATE\s+(\S+)\s+(.*?);$/)
      {
        # insert statement
        my $tableName = expandMacros($1);
        my $values    = expandMacros($2);

        my $insertName = getName("update");

        push(@definitions, "INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName));

        if    ($suffix eq "SQLITE")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "UPDATE $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "MARIADB")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "UPDATE $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }
        elsif ($suffix eq "POSTGRESQL")
        {
          print CFILE_HANDLE "#define INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE "UPDATE $tableName $values\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)." = INDEX_DEFINITION_INSERT_UPDATE_".$suffix."_".uc($insertName)."_;\n";
          print CFILE_HANDLE "\n";
        }

        $definition = "";
      }
      else
      {
        # other statements
        $definition = append($definition,expandMacros($line));

        if ($type eq "")
        {
          my $statementName = getName("statement");

          push(@definitions, "INDEX_DEFINITION_STATEMENT_".$suffix."_".uc($statementName));

          print CFILE_HANDLE "#define INDEX_DEFINITION_STATEMENT_".$suffix."_".uc($statementName)."_ \\\n";
          print CFILE_HANDLE "\"\\\n";
          print CFILE_HANDLE $definition."\\\n";
          print CFILE_HANDLE "\"\n";
          print CFILE_HANDLE "const char *INDEX_DEFINITION_STATEMENT_".$suffix."_".uc($statementName)." = INDEX_DEFINITION_STATEMENT_".$suffix."_".uc($statementName)."_;\n";
          print CFILE_HANDLE "\n";

          $definition = "";
        }
      }
    }
  }

  # close file
  close(HANDLE);

  # constants
  if ($suffix eq "")
  {
    foreach $name (keys %constants)
    {
      print HFILE_HANDLE "#define $PREFIX_CONST_NAME$name $constants{$name}\n";
    }
    print HFILE_HANDLE "\n";
  }

  if ($suffix ne "")
  {
    print HFILE_HANDLE "\n";

    # definitions
    print HFILE_HANDLE "extern const char * const INDEX_DEFINITIONS_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_COUNT_".$suffix." ".($#definitions+1)."\n";
    print HFILE_HANDLE "\n";

    print CFILE_HANDLE "const char * const INDEX_DEFINITIONS_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@definitions)
    {
       print CFILE_HANDLE "  ".$_."_,\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "\n";

    # tables
    print HFILE_HANDLE "extern const char * const INDEX_DEFINITION_TABLE_NAMES_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_TABLE_NAME_COUNT_".$suffix." ".($#tableNames+1)."\n";
    print HFILE_HANDLE "extern const char * const INDEX_DEFINITION_FTS_TABLE_NAMES_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_FTS_TABLE_NAME_COUNT_".$suffix." ".($#ftsTableNames+1)."\n";

    print CFILE_HANDLE "const char * const INDEX_DEFINITION_TABLE_NAMES_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@tableNames)
    {
       print CFILE_HANDLE "  \"$_\",\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "const char* const INDEX_DEFINITION_FTS_TABLE_NAMES_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@ftsTableNames)
    {
       print CFILE_HANDLE "  \"$_\",\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "\n";

    print HFILE_HANDLE "extern const char* const INDEX_DEFINITION_TABLES_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_TABLE_COUNT_".$suffix." ".($#tableNames+1)."\n";
    print HFILE_HANDLE "extern const char* const INDEX_DEFINITION_FTS_TABLES_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_FTS_TABLE_COUNT_".$suffix." ".($#ftsTableNames+1)."\n";
    print HFILE_HANDLE "\n";

    print CFILE_HANDLE "const char * const INDEX_DEFINITION_TABLES_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@tableNames)
    {
       print CFILE_HANDLE "  INDEX_DEFINITION_TABLE_".$suffix."_".uc($_)."_,\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "const char *const INDEX_DEFINITION_FTS_TABLES_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@ftsTableNames)
    {
       print CFILE_HANDLE "  INDEX_DEFINITION_TABLE_".$suffix."_".uc($_)."_,\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "\n";

    # indices
    print HFILE_HANDLE "extern const char* const INDEX_DEFINITION_INDEX_NAMES_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_INDEX_NAME_COUNT_".$suffix." ".($#indexNames+1)."\n";
    print HFILE_HANDLE "\n";

    print CFILE_HANDLE "const char* const INDEX_DEFINITION_INDEX_NAMES_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@indexNames)
    {
       print CFILE_HANDLE "  \"$_\",\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "\n";

    print HFILE_HANDLE "extern const char* const INDEX_DEFINITION_INDICES_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_INDICE_COUNT_".$suffix." ".($#indexNames+1)."\n";
    print HFILE_HANDLE "\n";

    print CFILE_HANDLE "const char* const INDEX_DEFINITION_INDICES_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@indexNames)
    {
       print CFILE_HANDLE "  INDEX_DEFINITION_INDEX_".$suffix."_".uc($_)."_,\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "\n";

    # triggers
    print HFILE_HANDLE "extern const char* const INDEX_DEFINITION_TRIGGER_NAMES_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_TRIGGER_NAME_COUNT_".$suffix." ".($#triggerNames+1)."\n";
    print HFILE_HANDLE "\n";
    print CFILE_HANDLE "const char* const INDEX_DEFINITION_TRIGGER_NAMES_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@triggerNames)
    {
       print CFILE_HANDLE "  \"$_\",\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "\n";

    print HFILE_HANDLE "extern const char* const INDEX_DEFINITION_TRIGGERS_".$suffix."[];\n";
    print HFILE_HANDLE "#define INDEX_DEFINITION_TRIGGER_COUNT_".$suffix." ".($#triggerNames+1+1)."\n";
    print HFILE_HANDLE "\n";

    print CFILE_HANDLE "const char* const INDEX_DEFINITION_TRIGGERS_".$suffix."[] = \n";
    print CFILE_HANDLE "{\n";
    foreach (@triggerNames)
    {
       print CFILE_HANDLE "  INDEX_DEFINITION_TRIGGER_".$suffix."_".uc($_)."_,\n";
    }
    print CFILE_HANDLE "  NULL\n";
    print CFILE_HANDLE "};\n";
    print CFILE_HANDLE "\n";

    print HFILE_HANDLE "\n";
  }
}

# ------------------------------ main program  -------------------------------

# get options
GetOptions("common=s"     => \$commonFileName,
           "sqlite=s"     => \$sqliteFileName,
           "mariadb=s"    => \$mariadbFileName,
           "postgresql=s" => \$postgresqlFileName,
           "source=s"     => \$cFileName,
           "header=s"     => \$hFileName,
           "help"         => \$help
          );

# help
if ($help == 1)
{
  print "Usage: $0 <options>\n";
  print "\n";
  print "Options: --common <file name>     - comon definition file\n";
  print "         --sqlite <file name>     - SqLite definition file\n";
  print "         --mariadb <file name>    - MariaDB definition file\n";
  print "         --postgresql <file name> - PostgreSQL definition file\n";
  print "         --source <file name>     - C source file\n";
  print "         --header <file name>     - C header file\n";
  print "         --help                   - output this help\n";
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

print HFILE_HANDLE "/* This file is auto-generated by index_definition.pl. Do NOT edit! */

#ifndef __INDEX_DEFINITION__
#define __INDEX_DEFINITION__

typedef const char *            IndexDefinition;
typedef IndexDefinition const * IndexDefinitions;

#define __INDEX_DEFINITIONS_ITERATE(__n, indexDefinitions, string) \\
  for (IndexDefinitions __GLOBAL_CONCAT(__iterator,__n) = indexDefinitions; (string) = *__GLOBAL_CONCAT(__iterator,__n), \\
       *__GLOBAL_CONCAT(__iterator,__n) != NULL; \\
       __GLOBAL_CONCAT(__iterator,__n)++, (string) = *__GLOBAL_CONCAT(__iterator,__n) \\
      )
#define INDEX_DEFINITIONS_ITERATE(indexDefinitions, string) \\
  __INDEX_DEFINITIONS_ITERATE(__COUNTER__,indexDefinitions,string)

#define __INDEX_DEFINITIONS_ITERATEX(__n, indexDefinitions, string, condition) \\
  for (IndexDefinitions __GLOBAL_CONCAT(__iterator,__n) = indexDefinitions; (string) = *__GLOBAL_CONCAT(__iterator,__n), \\
       (*__GLOBAL_CONCAT(__iterator,__n) != NULL) && (condition); \\
       __GLOBAL_CONCAT(__iterator,__n)++, (string) = *__GLOBAL_CONCAT(__iterator,__n) \\
      )
#define INDEX_DEFINITIONS_ITERATEX(indexDefinitions, string, condition) \\
  __INDEX_DEFINITIONS_ITERATEX(__COUNTER__,indexDefinitions, string, condition)

";
print CFILE_HANDLE "/* This file is auto-generated by index_definition.pl. Do NOT edit! */

#include <stdlib.h>

#include \"index_definition.h\"

";

processFile($commonFileName,"");
processFile($sqliteFileName,"SQLITE");
processFile($mariadbFileName,"MARIADB");
processFile($postgresqlFileName,"POSTGRESQL");

print HFILE_HANDLE "

extern const IndexDefinitions INDEX_DEFINITIONS[];
extern const IndexDefinitions INDEX_DEFINITION_TABLES[];
extern const IndexDefinitions INDEX_DEFINITION_TABLE_NAMES[];
extern const IndexDefinitions INDEX_DEFINITION_FTS_TABLES[];
extern const IndexDefinitions INDEX_DEFINITION_FTS_TABLE_NAMES[];
extern const IndexDefinitions INDEX_DEFINITION_INDICES[];
extern const IndexDefinitions INDEX_DEFINITION_INDEX_NAMES[];
extern const IndexDefinitions INDEX_DEFINITION_TRIGGERS[];
extern const IndexDefinitions INDEX_DEFINITION_TRIGGER_NAMES[];

#endif /* __INDEX_DEFINITION__ */
";
print CFILE_HANDLE "\
const IndexDefinitions INDEX_DEFINITIONS[] =
{
  INDEX_DEFINITIONS_SQLITE,
  INDEX_DEFINITIONS_MARIADB,
  INDEX_DEFINITIONS_POSTGRESQL
};
const IndexDefinitions INDEX_DEFINITION_TABLES[] =
{
  INDEX_DEFINITION_TABLES_SQLITE,
  INDEX_DEFINITION_TABLES_MARIADB,
  INDEX_DEFINITION_TABLES_POSTGRESQL
};
const IndexDefinitions INDEX_DEFINITION_TABLE_NAMES[] =
{
  INDEX_DEFINITION_TABLE_NAMES_SQLITE,
  INDEX_DEFINITION_TABLE_NAMES_MARIADB,
  INDEX_DEFINITION_TABLE_NAMES_POSTGRESQL
};
const IndexDefinitions INDEX_DEFINITION_FTS_TABLES[] =
{
  INDEX_DEFINITION_FTS_TABLES_SQLITE,
  INDEX_DEFINITION_FTS_TABLES_MARIADB,
  INDEX_DEFINITION_FTS_TABLES_POSTGRESQL
};
const IndexDefinitions INDEX_DEFINITION_FTS_TABLE_NAMES[] =
{
  INDEX_DEFINITION_FTS_TABLE_NAMES_SQLITE,
  INDEX_DEFINITION_FTS_TABLE_NAMES_MARIADB,
  INDEX_DEFINITION_FTS_TABLE_NAMES_POSTGRESQL
};
const IndexDefinitions INDEX_DEFINITION_INDICES[] =
{
  INDEX_DEFINITION_INDICES_SQLITE,
  INDEX_DEFINITION_INDICES_MARIADB,
  INDEX_DEFINITION_INDICES_POSTGRESQL
};
const IndexDefinitions INDEX_DEFINITION_INDEX_NAMES[] =
{
  INDEX_DEFINITION_INDEX_NAMES_SQLITE,
  INDEX_DEFINITION_INDEX_NAMES_MARIADB,
  INDEX_DEFINITION_INDEX_NAMES_POSTGRESQL
};
const IndexDefinitions INDEX_DEFINITION_TRIGGERS[] =
{
  INDEX_DEFINITION_TRIGGERS_SQLITE,
  INDEX_DEFINITION_TRIGGERS_MARIADB,
  INDEX_DEFINITION_TRIGGERS_POSTGRESQL
};
const IndexDefinitions INDEX_DEFINITION_TRIGGER_NAMES[] =
{
  INDEX_DEFINITION_TRIGGER_NAMES_SQLITE,
  INDEX_DEFINITION_TRIGGER_NAMES_MARIADB,
  INDEX_DEFINITION_TRIGGER_NAMES_POSTGRESQL
};
";

# close files
if ($cFileName ne "")
{
  close(CFILE_HANDLE);
}
if ($hFileName ne "")
{
  close(HFILE_HANDLE);
}

exit 0;
# end of file
