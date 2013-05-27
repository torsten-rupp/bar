#!/usr/bin/perl

#!/bin/sh
#\
#exec perl "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Revision$
# $Date$
# $Author$
# Contents: create header/c file definition from format definition
# Systems: all
#
# ----------------------------------------------------------------------------

# ---------------------------- additional packages ---------------------------
use English;
use POSIX;
use Getopt::Std;
use Getopt::Long;

# ---------------------------- constants/variables ---------------------------

my $PREFIX_CHUNK_ID         = "CHUNK_ID_";
my $PREFIX_CHUNK_NAME       = "Chunk";
my $PREFIX_CHUNK_DEFINITION = "CHUNK_DEFINITION_";

my $PREFIX_CONST_NAME       = "CHUNK_CONST_";

my $DEFINITION_TYPES =
  {
   "byte"   => "CHUNK_DATATYPE_BYTE",
   "uint8"  => "CHUNK_DATATYPE_UINT8",
   "uint16" => "CHUNK_DATATYPE_UINT16",
   "uint32" => "CHUNK_DATATYPE_UINT32",
   "uint64" => "CHUNK_DATATYPE_UINT64",
   "int8"   => "CHUNK_DATATYPE_INT8",
   "int16"  => "CHUNK_DATATYPE_INT16",
   "int32"  => "CHUNK_DATATYPE_INT32",
   "int64"  => "CHUNK_DATATYPE_INT64",
   "string" => "CHUNK_DATATYPE_STRING",

   "crc32"  => "CHUNK_DATATYPE_CRC32",

   "data"   => "CHUNK_DATATYPE_DATA",
  };

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

sub writeConstFile($)
{
  my $s=shift(@_);

  if ($constFileName ne "")
  {
    print CONSTFILE_HANDLE $s;
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
           "n=s" => \$constFileName,
           "i=s" => \@includeFileNames,
          );

if ($cFileName ne "")
{
  open(CFILE_HANDLE,"> $cFileName");
}
if ($hFileName ne "")
{
  open(HFILE_HANDLE,"> $hFileName");
  print HFILE_HANDLE "#ifndef __ARCHIVE_FORMAT__\n";
  print HFILE_HANDLE "#define __ARCHIVE_FORMAT__\n";

  print HFILE_HANDLE "#include \"lists.h\"\n";
  print HFILE_HANDLE "#include \"chunks.h\"\n";
  print HFILE_HANDLE "#include \"crypt.h\"\n";
  print HFILE_HANDLE "#include \"compress.h\"\n";
}
if ($constFileName ne "")
{
  open(CONSTFILE_HANDLE,"> $constFileName");
  print CONSTFILE_HANDLE "#ifndef __ARCHIVE_FORMAT_CONST__\n";
  print CONSTFILE_HANDLE "#define __ARCHIVE_FORMAT_CONST__\n";
}

writeCFile("#include \"chunks.h\"\n");
if (scalar(@includeFileNames) > 0)
{
  for my $includeFileName (@includeFileNames)
  {
    writeCFile("#include \"$includeFileName\"\n");
  }
}

my $line;
my $lineNb=0;
while ($line=<STDIN>)
{
  chop $line;
  $lineNb++;
  if ($line =~ /^\s*#/ || $line =~ /^\s*$/) { next; }
#print "$line\n";

  if    ($line =~ /^CHUNK\s+(\w+)\s+"(.*)"\s+(\w+)/)
  {
    # chunk
    my $n=0;
    my $idName=$1;
    my $id=$2;
    my $structName=$3;
    my @parseDefinitions;

    # Note: use padding in C structures for access via pointer

    writeHFile("#define $PREFIX_CHUNK_ID$idName (('".substr($id,0,1)."' << 24) | ('".substr($id,1,1)."' << 16) | ('".substr($id,2,1)."' << 8) | '".substr($id,3,1)."')\n");
    writeHFile("typedef struct $PREFIX_CHUNK_NAME$structName\n");
    writeHFile("{\n");
    writeHFile("  LIST_NODE_HEADER(struct $PREFIX_CHUNK_NAME$structName);\n");
    writeHFile("  ChunkInfo info;\n");
    while ($line=<STDIN>)
    {
      # get line
      chop $line;
      $lineNb++;
      if ($line =~ /^\s*#/) { next; }

      # check end of block
      if ($line =~ /^\s*$/) { last; }

      # parse
      if    ($line =~ /^\s*ENCRYPT\s*$/)
      {
        writeHFile("  CryptInfo cryptInfo;\n");
      }
      elsif ($line =~ /^\s*COMPRESS\s*$/)
      {
        writeHFile("  CompressInfo compressInfo;\n");
      }
      elsif ($line =~ /^\s*(byte|uint8|int8)\s+(\w+)/)
      {
        writeHFile("  $1 $2;\n");
        writeHFile("  uint8 pad".$n."[3];\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2)");
      }
      elsif ($line =~ /^\s*(uint16|int16)\s+(\w+)/)
      {
        writeHFile("  $1 $2;\n");
        writeHFile("  uint8 pad".$n."[2];\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2)");
      }
      elsif ($line =~ /^\s*(uint32|int32)\s+(\w+)/)
      {
        writeHFile("  $1 $2;\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2)");
      }
      elsif ($line =~ /^\s*(uint64|int64)\s+(\w+)/)
      {
        writeHFile("  $1 $2;\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2)");
      }
      elsif ($line =~ /^\s*string\s+(\w+)/)
      {
        writeHFile("  String $1;\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{string});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$1)");
      }
      elsif ($line =~ /^\s*(byte|uint8|int8|uint16|int16|uint32|int32|uint64|int64|string)\[\]\s+(\w+)/)
      {
        writeHFile("  struct\n");
        writeHFile("  {\n");
        writeHFile("    uint length;\n");
        writeHFile("    $1 *data;\n");
        writeHFile("  } $2;\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1}."|CHUNK_DATATYPE_ARRAY");
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2.length)");
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2.data)");
      }
      elsif ($line =~ /^\s*crc32\s+(\w+)/)
      {
        push(@parseDefinitions,$DEFINITION_TYPES->{crc32});
        push(@parseDefinitions,"0");
      }
      elsif ($line =~ /^\s*data\s+(\w+)/)
      {
        push(@parseDefinitions,$DEFINITION_TYPES->{data});
        push(@parseDefinitions,"0");
      }
      else
      {
        print STDERR "Unknown data '$line' in line $lineNb\n";
        exit 1;
      }

      $n++;
    }
    writeHFile("} $PREFIX_CHUNK_NAME$structName;\n");
    writeHFile("typedef struct { LIST_HEADER($PREFIX_CHUNK_NAME$structName); } $PREFIX_CHUNK_NAME$structName"."List".";\n");

    push(@parseDefinitions,"CHUNK_DATATYPE_NONE");
    writeHFile("extern const int $PREFIX_CHUNK_DEFINITION$idName\[\];\n");
    writeCFile("const int $PREFIX_CHUNK_DEFINITION$idName\[\] = {".join(",",@parseDefinitions)."};\n");
  }
  elsif ($line =~ /^CONST\s+(\w+)\s*=\s*(\S*)\s*/)
  {
    # constant
    my $name =$1;
    my $value=$2;

    writeConstFile("#define $PREFIX_CONST_NAME$name $value\n");
  }
  else
  {
    print STDERR "Unknown data '$line' in line $lineNb\n";
    exit 1;
  }
}

if ($constFileName ne "")
{
  print CONSTFILE_HANDLE "#endif /* __ARCHIVE_FORMAT_CONST__ */\n";
  close(CONSTFILE_HANDLE);
}
if ($hFileName ne "")
{
  print HFILE_HANDLE "#endif /* __ARCHIVE_FORMAT__ */\n";
  close(HFILE_HANDLE);
}
if ($cFileName ne "")
{
  close(CFILE_HANDLE);
}

exit 0;
# end of file
