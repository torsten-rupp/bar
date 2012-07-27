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
      print "\n";
      print " -- Torsten Rupp <torsten.rupp\@gmx.net>  $date\n";
      $changeLogFlag=0;
    }
    else
    {
      print "$line\n";
    }
  }
  else
  {
    if    ($line =~ /^(\S+)\s+(.*)\s*$/)
    {
      print "bar ($2) stable; urgency=low\n";
      open(HANDLE,"date -d $1 '+%a, %d %b %Y %H:%M:%S %z'|");
      read(HANDLE,$date,1024);
      close(HANDLE);
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
