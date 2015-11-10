#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/osc/changelog.pl,v $
# $Revision: 1.1 $
# $Author: torsten $
# Contents: convert ChangeLog to Debian change log
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

GetOptions ("type=s" => \$type,
            "help"   => \$help
           );
if    ($type eq "debian")
{
}
elsif ($type eq "spec")
{
}
else
{
  print STDERR "Unknown type '$type'!\n";
  exit 1;
}

my $line;
my $lineNb=0;
my $changeLogFlag=0;
my $date="";
while ($line=<STDIN>)
{
  chop $line;
  $lineNb++;
#print "$line\n";

  if ($changeLogFlag)
  {
    if    ($line =~ /^\s*$/)
    {
      if    ($type eq "debian")
      {
        print "\n";
        print " -- Torsten Rupp <torsten.rupp\@gmx.net>  $date\n";
        print "\n";
      }
      elsif ($type eq "spec")
      {
        print "\n";
      }

      $changeLogFlag=0;
    }
    elsif ($line =~ /\s*\*\s*(\S.*)/)
    {
      if    ($type eq "debian")
      {
        print "  * $1\n";
      }
      elsif ($type eq "spec")
      {
        $s = $1;
        $s =~ s/%/%%/g;
        print "  - $s\n";
      }
    }
    elsif ($line =~ /\s*(\S.*)/)
    {
      $s = $1;
      $s =~ s/%/%%/g;
      print "    $s\n";
    }
    else
    {
    }
  }
  else
  {
    if    ($line =~ /^(\S+)\s+(\S+).*\s*$/)
    {
      if    ($type eq "debian")
      {
        open(HANDLE,"date -d $1 '+%a, %d %b %Y %H:%M:%S %z'|");
        read(HANDLE,$date,1024);
        close(HANDLE);
        chop $date;

        print "bar ($2) stable; urgency=low\n";
      }
      elsif ($type eq "spec")
      {
        open(HANDLE,"date -d $1 '+%a %b %d %Y'|");
        read(HANDLE,$date,1024);
        close(HANDLE);
        chop $date;

        print "* $date Torsten Rupp <torsten.rupp\@gmx.net> $2\n";
      }
      $changeLogFlag=1;
    }
    elsif (($line =~ /^\s*$/) || ($line =~ /^\s*\/\//))
    {
      next;
    }
    else
    {
      print STDERR "Unknown data '$line' in line $lineNb\n";
      exit 1;
    }
  }
}
if ($changeLogFlag)
{
  print "\n";
  print " -- Torsten Rupp <torsten.rupp\@gmx.net>  $date\n";
}

exit 0;
# end of file
