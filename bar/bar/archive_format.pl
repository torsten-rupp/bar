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
my $PREFIX_CHUNK_FIXE_SIZE  = "CHUNK_FIXED_SIZE_";

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

my $definitionFileName;
my $cFileName,$hFileName;
my %idNameMap;
my %structNameMap;
my @transformations;

# --------------------------------- includes ---------------------------------

# -------------------------------- functions ---------------------------------

sub align($$)
{
  my $n        =shift(@_);
  my $alignment=shift(@_);

  return ($n+$alignment-1) & ~($alignment-1);
}

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

$definitionFileName=$ARGV[0];
if ($ARGV[0] ne "")
{
  open(STDIN,"< $definitionFileName");
}
else
{
  print STDERR "Warning: not definition file name given - read from stdin\n";
}

if ($cFileName ne "")
{
  open(CFILE_HANDLE,"> $cFileName");
}
if ($hFileName ne "")
{
  open(HFILE_HANDLE,"> $hFileName");
  print HFILE_HANDLE "#ifndef __ARCHIVE_FORMAT__\n";
  print HFILE_HANDLE "#define __ARCHIVE_FORMAT__\n";
  print HFILE_HANDLE "\n";

  print HFILE_HANDLE "#include \"lists.h\"\n";
  print HFILE_HANDLE "#include \"chunks.h\"\n";
  print HFILE_HANDLE "#include \"crypt.h\"\n";
  print HFILE_HANDLE "#include \"compress.h\"\n";
  print HFILE_HANDLE "\n";
}
if ($constFileName ne "")
{
  open(CONSTFILE_HANDLE,"> $constFileName");
  print CONSTFILE_HANDLE "#ifndef __ARCHIVE_FORMAT_CONST__\n";
  print CONSTFILE_HANDLE "#define __ARCHIVE_FORMAT_CONST__\n";
  print CONSTFILE_HANDLE "\n";
}

writeCFile("#include \"chunks.h\"\n");
if (scalar(@includeFileNames) > 0)
{
  for my $includeFileName (@includeFileNames)
  {
    writeCFile("#include \"$includeFileName\"\n");
  }
}
writeCFile("\n");

my $line;
my $lineNb=0;
while ($line=<STDIN>)
{
  chop $line;
  $lineNb++;
  if ($line =~ /^\s*#/ || $line =~ /^\s*$/) { next; }
#print "$line\n";

  if    ($line =~ /^CHUNK\s+(\w+)\s+"(.*)"\s+(\w+)(\s+(\w+)){0,1}/)
  {
    # chunk
    my $idName     = $1;
    my $id         = $2;
    my $structName = $3;
    my @attributes = split(/,/,$5);

    my $n         = 0;
    my @parseDefinitions;
    my $fixedSize = 0;

    # save id/struct name
    $idNameMap{$id} = $idName;
    $structNameMap{$id} = $structName;

    # check attributes
    for my $attribute (@attributes)
    {
#print STDERR "attribute=$attribute\n";
      if    ($attribute eq "")
      {
        # nothing to do
      }
      elsif ($attribute eq "DEPRECATED")
      {
        $idNameMap{$id} = $idName.$n;
        $structNameMap{$id} = $structName.$n;
        $idName=$idName.$n;
        $structName=$structName.$n;
      }
      else
      {
        print STDERR "Unknown attribute '$attribute' in '$line' in line $lineNb\n";
        exit 1;
      }
    }

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
      if    ($line =~ /^\s*ENCRYPT\s*\[(\d+)\]\s*$/)
      {
        writeHFile("  CryptInfo cryptInfos[$1];\n");
      }
      elsif ($line =~ /^\s*ENCRYPT\s*$/)
      {
        writeHFile("  CryptInfo cryptInfo;\n");
      }
      elsif ($line =~ /^\s*COMPRESS\s*$/)
      {
        writeHFile("  CompressInfo compressInfo;\n");
      }
      elsif ($line =~ /^\s*ALIGN\s+(\w+)$/)
      {
        if ($1 == 0)
        {
        }
        elsif (($1 == 2) || ($1 == 4) || ($1 == 8) || ($1 == 16))
        {
          push(@parseDefinitions,"CHUNK_ALIGN");
          push(@parseDefinitions,$1);
        }
        else
        {
          print STDERR "Invalid alignment '$1' in line $lineNb\n";
          exit 1;
        }
      }
      elsif ($line =~ /^\s*(byte|uint8|int8)\s*\[(\d+)\]\s+(\w+)/)
      {
        writeHFile("  $1 $3\[$2\];\n");
        if ($2%4 > 0) { writeHFile("  uint8 pad".$n."[".($2%4)."];\n"); }
        push(@parseDefinitions,$DEFINITION_TYPES->{$1}."|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED");
        push(@parseDefinitions,"$2");
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$3)");
        $fixedSize += $2*1;
      }
      elsif ($line =~ /^\s*(byte|uint8|int8)\s+(\w+)/)
      {
        writeHFile("  $1 $2;\n");
        writeHFile("  uint8 pad".$n."[3];\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2)");
        $fixedSize += 1;
      }
      elsif ($line =~ /^\s*(uint16|int16)\s*\[(\d+)\]\s+(\w+)/)
      {
        writeHFile("  $1 $3\[$2\];\n");
        if ($2%2 > 0) { writeHFile("  uint8 pad".$n."[".($2%2*2)."];\n"); }
        push(@parseDefinitions,$DEFINITION_TYPES->{$1}."|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED");
        push(@parseDefinitions,"$2");
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$3)");
        $fixedSize += $2*2;
      }
      elsif ($line =~ /^\s*(uint16|int16)\s+(\w+)/)
      {
        writeHFile("  $1 $2;\n");
        writeHFile("  uint8 pad".$n."[2];\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2)");
        $fixedSize += 2;
      }
      elsif ($line =~ /^\s*(uint32|int32)\s*\[(\d+)\]\s+(\w+)/)
      {
        writeHFile("  $1 $3\[$2\];\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1}."|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED");
        push(@parseDefinitions,"$2");
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$3)");
        $fixedSize += $2*4;
      }
      elsif ($line =~ /^\s*(uint32|int32)\s+(\w+)/)
      {
        writeHFile("  $1 $2;\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2)");
        $fixedSize += 4;
      }
      elsif ($line =~ /^\s*(uint64|int64)\s*\[(\d+)\]\s+(\w+)/)
      {
        writeHFile("  $1 $3\[$2\];\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1}."|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED");
        push(@parseDefinitions,"$2");
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$3)");
        $fixedSize += $2*8;
      }
      elsif ($line =~ /^\s*(uint64|int64)\s+(\w+)/)
      {
        writeHFile("  $1 $2;\n");
        push(@parseDefinitions,$DEFINITION_TYPES->{$1});
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2)");
        $fixedSize += 8;
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
        push(@parseDefinitions,$DEFINITION_TYPES->{$1}."|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC");
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2.length)");
        push(@parseDefinitions,"offsetof($PREFIX_CHUNK_NAME$structName,$2.data)");
      }
      elsif ($line =~ /^\s*crc32\s+(\w+)/)
      {
        push(@parseDefinitions,$DEFINITION_TYPES->{crc32});
        push(@parseDefinitions,"0");
        $fixedSize += 4;
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
    writeHFile("extern const ChunkDefinition $PREFIX_CHUNK_DEFINITION$idName\[\];\n");
    writeHFile("#define $PREFIX_CHUNK_FIXE_SIZE$idName $fixedSize\n");
    writeHFile("\n");
    writeCFile("const ChunkDefinition $PREFIX_CHUNK_DEFINITION$idName\[\] = {".join(",",@parseDefinitions)."};\n");
    writeCFile("\n");
  }
  elsif ($line =~ /^TRANSFORM\s+"(.*)"\s+"(.*)"\s*/)
  {
    # TRANSFORM
    my $n=0;
    my $oldId=$1;
    my $newId=$2;

    # check ids
    if (!$structNameMap{$oldId})
    {
      print STDERR "Deprecated id '$oldId' not found!\n";
      exit 1;
    }
    if (!$structNameMap{$newId})
    {
      print STDERR "Id '$newId' not found!\n";
      exit 1;
    }

    # get code block
    $line=<STDIN>;
    chop $line;
    $lineNb++;
    if ($line !~ /^\s*{\s*$/)
    {
      print STDERR "Excpected '{' in line $lineNb\n";
      exit 1;
    }
    writeCFile("LOCAL Errors transform_$structNameMap{$oldId}(Chunk$structNameMap{$oldId} *OLD, Chunk$structNameMap{$newId} *NEW, void *userData)\n");
    writeCFile("{\n");
    writeCFile("#line ".($lineNb+1)." \"$definitionFileName\"\n");
    while ($line=<STDIN>)
    {
      # get line
      chop $line;
      $lineNb++;
      if ($line =~ /^\s*#/) { next; }
      writeCFile("$line\n");

      # check end of block
      if ($line =~ /^\s*}\s*$/) { last; }
    }
    writeCFile("\n");

    my $transformation = { oldId => $oldId, newId => $newId };
    push(@transformations,$transformation);
  }
  elsif ($line =~ /^const\s+(\w+)\s*=\s*(\S*)\s*/)
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

# ".($#transformations+1)."
writeHFile("extern const ChunkTransformInfo CHUNK_TRANSFORM_INFOS[];\n");
writeCFile("\n");

writeCFile("const ChunkTransformInfo CHUNK_TRANSFORM_INFOS[] =\n");
writeCFile("{\n");
for my $transformation (@transformations)
{
  writeCFile("  { { CHUNK_ID_$idNameMap{$transformation->{oldId}}, \n".
             "      sizeof($PREFIX_CHUNK_NAME$structNameMap{$transformation->{oldId}}),\n".
             "      $PREFIX_CHUNK_FIXE_SIZE$idNameMap{$transformation->{oldId}}, \n".
             "      $PREFIX_CHUNK_DEFINITION$idNameMap{$transformation->{oldId}}, \n".
             "    },\n".
             "    { CHUNK_ID_$idNameMap{$transformation->{newId}}, \n".
             "      sizeof($PREFIX_CHUNK_NAME$structNameMap{$transformation->{newId}}),\n".
             "      $PREFIX_CHUNK_FIXE_SIZE$idNameMap{$transformation->{newId}}, \n".
             "      $PREFIX_CHUNK_DEFINITION$idNameMap{$transformation->{newId}}, \n".
             "    },\n".
             "    (ChunkTransformFunction)transform_$structNameMap{$transformation->{oldId}}\n".
             "  },\n"
            );
}
writeCFile("  { { CHUNK_ID_NONE,0,0,NULL },\n".
           "    { CHUNK_ID_NONE,0,0,NULL },\n".
           "    NULL\n".
           "  }\n"
          );
writeCFile("};\n");

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
