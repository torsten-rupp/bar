#!/bin/bash

SQLITE3=./bar-sqlite3
make $SQLITE3 > /dev/null

databaseFile=bar-index.db
testBase=1
testDelete=1
testAssign=1
testDirectory=1
testFTS=1

DATETIME1=`date -d "2016-01-01 01:01:01" +%s`
DATETIME2=`date -d "2016-02-02 02:02:02" +%s`
DATETIME3=`date -d "2016-03-03 03:03:03" +%s`
DATETIME4=`date -d "2016-04-04 04:04:04" +%s`

TYPE_FILE=5
TYPE_IMAGE=6
TYPE_DIRECTORY=7
TYPE_LINK=8
TYPE_HARDLINK=9
TYPE_SPECIAL=10

jobUUID1="1111"
jobUUID2="2222"
jobUUID3="3333"

uuidId1=0
uuidId2=0
uuidId3=0

entityId1=0
entityId2=0
entityId3=0

storageId1=0
storageId2=0
storageId3=0

function resetDatabase
{
  $SQLITE3 $databaseFile -c

  # uuid
  uuidId1=`$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('$jobUUID1'); SELECT last_insert_rowid();"`
  uuidId2=`$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('$jobUUID2'); SELECT last_insert_rowid();"`
  uuidId3=`$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('$jobUUID3'); SELECT last_insert_rowid();"`

  # entity
  entityId1=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('$jobUUID1','',$DATETIME1,1); SELECT last_insert_rowid();"`
  entityId2=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('$jobUUID2','',$DATETIME1,1); SELECT last_insert_rowid();"`
  entityId3=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('$jobUUID3','',$DATETIME1,1); SELECT last_insert_rowid();"`
  verify "SELECT COUNT(id) FROM entities" 4

  # storage
  storageId1=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId1,'s1',$DATETIME2,4400,1,1); SELECT last_insert_rowid();"`
  storageId2=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId2,'s2',$DATETIME3,400,1,1); SELECT last_insert_rowid();"`
  storageId3=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId3,'s3',$DATETIME4,0,1,1); SELECT last_insert_rowid();"`
  verify "SELECT COUNT(id) FROM storage" 3
}

function insertFile
{
  local storageId=$1;
  local name="$2"
  local time=$3;
  local size=$4; if test -z "$size"; then size=1000; fi
  local fragmentOffset=$5; if test -z "$fragmentOffset"; then fragmentOffset=0; fi
  local fragmentSize=$6; if test -z "$fragmentSize"; then fragmentSize=0; fi
  local id;

  # insert 1 file fragment into storage
  id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId,$TYPE_FILE,'$name',$time); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId,$id,$size,$fragmentOffset,$fragmentSize)"

  echo $id
}

function insertImage
{
  local storageId=$1;
  local name="$2"
  local time=$3;
  local size=$4; if test -z "$size"; then size=1000; fi
  local fragmentOffset=$5; if test -z "$fragmentOffset"; then fragmentOffset=0; fi
  local fragmentSize=$6; if test -z "$fragmentSize"; then fragmentSize=0; fi
  local id;

  # insert 1 image entry
  id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId,$TYPE_IMAGE,'$name',$time); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId,$id,$size,1,$fragmentOffset,$fragmentSize)"

  echo $id
}

function insertDirectory
{
  local storageId=$1;
  local name="$2"
  local time=$3;
  local id;

  # insert 1 directory entry
  id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId,$TYPE_DIRECTORY,'$name',$time); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId,$id,$storageId,'$name')"

  echo $id
}

function insertLink
{
  local storageId=$1;
  local name="$2"
  local time=$3;
  local id;

  # insert 1 link entry
  id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId,$TYPE_LINK,'$name',$time); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId,$id,'destination1')"

  echo $id
}

function insertHardlink
{
  local storageId=$1;
  local name="$2"
  local time=$3;
  local size=$4; if test -z "$size"; then size=1000; fi
  local fragmentOffset=$5; if test -z "$fragmentOffset"; then fragmentOffset=0; fi
  local fragmentSize=$6; if test -z "$fragmentSize"; then fragmentSize=0; fi
  local id;

  # insert 1 hardlink fragment into storage
  id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId,$TYPE_HARDLINK,'$name',$time); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId,$id,$size,$fragmentOffset,$fragmentSize)"

  echo $id
}

function insertSpecial
{
  local storageId=$1;
  local name="$2"
  local time=$3;
  local id;

  # insert 1 special entry
  id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId,$TYPE_SPECIAL,'special',$time); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId,$id,1)"

  echo $id
}

function verify
{
  local sqlCommand=$1;
  local expected=$2;

  local value=`$SQLITE3 $databaseFile "$sqlCommand"`
  if test -z "$value"; then
    echo >&2 "ERROR at `caller`: expected $expected, got nothing: $sqlCommand"
    exit 1
  fi
  if test "$value" -ne "$expected"; then
    echo >&2 "ERROR at `caller`: expected $expected, got $value: $sqlCommand"
    exit 1
  fi
}

function printValues
{
  echo "--- UUIDs: -----------------------------------------------------------"
  $SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize,totalFileCount,totalFileSize FROM uuids"
  echo "--- Entities: --------------------------------------------------------"
  $SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
  $SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM entities"
  echo "--- Storage: ---------------------------------------------------------"
  $SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
  $SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM storage"
  $SQLITE3 $databaseFile -H "SELECT id,totalFileCountNewest,totalFileSizeNewest,totalImageCountNewest,totalImageSizeNewest,totalDirectoryCountNewest,totalLinkCountNewest,totalHardlinkCountNewest,totalHardlinkSizeNewest,totalSpecialCountNewest FROM storage"
  echo "--- Entries: ---------------------------------------------------------"
  $SQLITE3 $databaseFile -H "SELECT id,storageId,name,type,timeLastChanged,offset,size FROM entries"
  echo "--- Newest entries: --------------------------------------------------"
  $SQLITE3 $databaseFile -H "SELECT id,entryId,storageId,entryId,name,type,offset,size FROM entriesNewest"
  echo "--- Directory entries: -----------------------------------------------"
  $SQLITE3 $databaseFile -H "SELECT id,storageId,name,totalEntryCount,totalEntrySize FROM directoryEntries"
}

function log
{
  $SQLITE3 $databaseFile "INSERT INTO log VALUES('$0')"
}

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


if test $testBase -eq 1; then
  resetDatabase

  # files
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storage1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storage1TotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entities1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,500,500)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,500,500)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,500,500)"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+6))
    verify "SELECT COUNT(id) FROM fileEntries" 6

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCount+6))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCountNewest+4))
    verify "SELECT totalFileCount FROM storage WHERE id=$storageId1" 6
    verify "SELECT totalFileSize FROM storage WHERE id=$storageId1" 3000
    verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId1" 4
    verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId1" 2000

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entities1TotalEntryCount+6))
    verify "SELECT totalFileCount FROM entities WHERE id=$entityId1" 6
    verify "SELECT totalFileSize FROM entities WHERE id=$entityId1" 3000
  fi

  # images
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storage1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storage1TotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entities1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId1,$id,1000,1,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId1,$id,1000,1,500,500)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId1,$id,1000,1,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId1,$id,1000,1,500,500)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId1,$id,1000,1,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId1,$id,1000,1,500,500)"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+6))
    verify "SELECT COUNT(id) FROM imageEntries" 6

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCount+6))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCountNewest+4))
    verify "SELECT totalImageCount FROM storage WHERE id=$storageId1" 6
    verify "SELECT totalImageSize FROM storage WHERE id=$storageId1" 3000
    verify "SELECT totalImageCountNewest FROM storage WHERE id=$storageId1" 4
    verify "SELECT totalImageSizeNewest FROM storage WHERE id=$storageId1" 2000

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entities1TotalEntryCount+6))
    verify "SELECT totalImageCount FROM entities WHERE id=$entityId1" 6
    verify "SELECT totalImageSize FROM entities WHERE id=$entityId1" 3000
  fi

  # directories
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storage1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storage1TotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entities1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir1')"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir2')"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir2')"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+3))
    verify "SELECT COUNT(id) FROM directoryEntries" 3;

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCount+3))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCountNewest+2))
    verify "SELECT totalDirectoryCount FROM storage WHERE id=$storageId1" 3
    verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId1" 2

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entities1TotalEntryCount+3))
    verify "SELECT totalDirectoryCount FROM entities WHERE id=$entityId1" 3
  fi

  # links
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storage1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storage1TotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entities1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination1')"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination2')"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination2')"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+3));
    verify "SELECT COUNT(id) FROM linkEntries" 3;

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCount+3))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCountNewest+2))
    verify "SELECT totalLinkCount FROM storage WHERE id=$storageId1" 3
    verify "SELECT totalLinkCountNewest FROM storage WHERE id=$storageId1" 2

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entities1TotalEntryCount+3))
    verify "SELECT totalLinkCount FROM entities WHERE id=$entityId1" 3
  fi

  # hardlink
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storage1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storage1TotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entities1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,500,500)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,500,500)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,0,500)"
    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id,1000,500,500)"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+6))
    verify "SELECT COUNT(id) FROM hardlinkEntries" 6

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCount+6))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCountNewest+4))
    verify "SELECT totalHardlinkCount FROM storage WHERE id=$storageId1" 6
    verify "SELECT totalHardlinkSize FROM storage WHERE id=$storageId1" 3000
    verify "SELECT totalHardlinkCountNewest FROM storage WHERE id=$storageId1" 4
    verify "SELECT totalHardlinkSizeNewest FROM storage WHERE id=$storageId1" 2000

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entities1TotalEntryCount+6))
    verify "SELECT totalHardlinkCount FROM entities WHERE id=$entityId1" 6
  fi

  # special
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storage1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storage1TotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entities1TotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,1)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,2)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,2)"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+3))
    verify "SELECT COUNT(id) FROM specialEntries" 3

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCount+3))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storage1TotalEntryCountNewest+2))
    verify "SELECT totalSpecialCount FROM storage WHERE id=$storageId1" 3
    verify "SELECT totalSpecialCountNewest FROM storage WHERE id=$storageId1" 2

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entities1TotalEntryCount+3))
    verify "SELECT totalSpecialCount FROM entities WHERE id=$entityId1" 3
  fi

  printValues
  echo "----------------------------------------------------------------------"
fi

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if test $testDelete -eq 1; then
  # insert+delete
  resetDatabase

  # insert 2 file fragments into storage2
  insertFile $storageId2 "file1" $DATETIME1 1000   0 500 > /dev/null
  insertFile $storageId2 "file1" $DATETIME1 1000 500 500 > /dev/null
  # insert 2 file fragments with newer date into storage2
  id1=`insertFile $storageId2 "file1" $DATETIME2 1000   0 500`
  id2=`insertFile $storageId2 "file1" $DATETIME2 1000 500 500`

  id3=`insertImage $storageId2 "image1" $DATETIME1 1000 0 500`

  id4=`insertDirectory $storageId2 "directory1" $DATETIME1`

  id5=`insertLink $storageId2 "link1" $DATETIME1`

  id6=`insertHardlink $storageId2 "hardlink1" $DATETIME1 1000 0 500`

  id7=`insertSpecial $storageId2 "special1" $DATETIME1`

  echo "--- after insert -----------------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id) FROM entries" $((4+1+1+1+1+1))

  verify "SELECT totalEntryCount       FROM storage WHERE id=$storageId2" $((4+1+1+1+1+1))
  verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId2" $((2+1+1+1+1+1))

  verify "SELECT totalEntryCount       FROM entities WHERE id=$entityId2" $((4+1+1+1+1+1))

  verify "SELECT COUNT(id) FROM fileEntries"      4
  verify "SELECT COUNT(id) FROM imageEntries"     1
  verify "SELECT COUNT(id) FROM directoryEntries" 1
  verify "SELECT COUNT(id) FROM linkEntries"      1
  verify "SELECT COUNT(id) FROM hardlinkEntries"  1
  verify "SELECT COUNT(id) FROM specialEntries"   1

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId2" 9

  verify "SELECT totalFileCount            FROM storage WHERE id=$storageId2" 4
  verify "SELECT totalFileSize             FROM storage WHERE id=$storageId2" 2000
  verify "SELECT totalFileCountNewest      FROM storage WHERE id=$storageId2" 2
  verify "SELECT totalFileSizeNewest       FROM storage WHERE id=$storageId2" 1000

  verify "SELECT totalImageCount           FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalImageSize            FROM storage WHERE id=$storageId2" 500
  verify "SELECT totalImageCountNewest     FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalImageSizeNewest      FROM storage WHERE id=$storageId2" 500

  verify "SELECT totalDirectoryCount       FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId2" 1

  verify "SELECT totalLinkCount            FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalLinkCountNewest      FROM storage WHERE id=$storageId2" 1

  verify "SELECT totalHardlinkCount        FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalHardlinkSize         FROM storage WHERE id=$storageId2" 500
  verify "SELECT totalHardlinkCountNewest  FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalHardlinkSizeNewest   FROM storage WHERE id=$storageId2" 500

  verify "SELECT totalSpecialCount         FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalSpecialCountNewest   FROM storage WHERE id=$storageId2" 1

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2" 9
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId2" 3000
  verify "SELECT totalFileCount            FROM entities WHERE id=$entityId2" 4
  verify "SELECT totalFileSize             FROM entities WHERE id=$entityId2" 2000
  verify "SELECT totalImageCount           FROM entities WHERE id=$entityId2" 1
  verify "SELECT totalImageSize            FROM entities WHERE id=$entityId2" 500
  verify "SELECT totalHardlinkCount        FROM entities WHERE id=$entityId2" 1
  verify "SELECT totalHardlinkSize         FROM entities WHERE id=$entityId2" 500

  verify "SELECT totalEntryCount           FROM uuids WHERE id=$uuidId2" 9
  verify "SELECT totalEntrySize            FROM uuids WHERE id=$uuidId2" 3000
  verify "SELECT totalFileCount            FROM uuids WHERE id=$uuidId2" 4
  verify "SELECT totalFileSize             FROM uuids WHERE id=$uuidId2" 2000
  verify "SELECT totalImageCount           FROM uuids WHERE id=$uuidId2" 1
  verify "SELECT totalImageSize            FROM uuids WHERE id=$uuidId2" 500
  verify "SELECT totalHardlinkCount        FROM uuids WHERE id=$uuidId2" 1
  verify "SELECT totalHardlinkSize         FROM uuids WHERE id=$uuidId2" 500

  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id7"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id6"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id5"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id4"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id3"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id2"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id1"

  echo "--- after delete -----------------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id)                 FROM entries"                       2
  verify "SELECT totalEntryCount           FROM storage  WHERE id=$storageId2" 2
  verify "SELECT totalEntryCountNewest     FROM storage  WHERE id=$storageId2" 2
  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2"  2

  verify "SELECT COUNT(id)                 FROM fileEntries"                   2
  verify "SELECT COUNT(id)                 FROM imageEntries"                  0
  verify "SELECT COUNT(id)                 FROM directoryEntries"              0
  verify "SELECT COUNT(id)                 FROM linkEntries"                   0
  verify "SELECT COUNT(id)                 FROM hardlinkEntries"               0
  verify "SELECT COUNT(id)                 FROM specialEntries"                0

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId2"  2
  verify "SELECT totalFileCount            FROM storage WHERE id=$storageId2"  2
  verify "SELECT totalFileSize             FROM storage WHERE id=$storageId2"  1000
  verify "SELECT totalFileCountNewest      FROM storage WHERE id=$storageId2"  2
  verify "SELECT totalFileSizeNewest       FROM storage WHERE id=$storageId2"  1000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2"  2
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId2"  1000
  verify "SELECT totalFileCount            FROM entities WHERE id=$entityId2"  2
  verify "SELECT totalFileSize             FROM entities WHERE id=$entityId2"  1000
  verify "SELECT totalImageCount           FROM entities WHERE id=$entityId2"  0
  verify "SELECT totalImageSize            FROM entities WHERE id=$entityId2"  0
  verify "SELECT totalHardlinkCount        FROM entities WHERE id=$entityId2"  0
  verify "SELECT totalHardlinkSize         FROM entities WHERE id=$entityId2"  0

  verify "SELECT totalEntryCount           FROM uuids WHERE id=$uuidId2"       2
  verify "SELECT totalEntrySize            FROM uuids WHERE id=$uuidId2"       1000
  verify "SELECT totalFileCount            FROM uuids WHERE id=$uuidId2"       2
  verify "SELECT totalFileSize             FROM uuids WHERE id=$uuidId2"       1000
  verify "SELECT totalImageCount           FROM uuids WHERE id=$uuidId2"       0
  verify "SELECT totalImageSize            FROM uuids WHERE id=$uuidId2"       0
  verify "SELECT totalHardlinkCount        FROM uuids WHERE id=$uuidId2"       0
  verify "SELECT totalHardlinkSize         FROM uuids WHERE id=$uuidId2"       0
fi

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if test $testAssign -eq 1; then
  # assign storage
  resetDatabase

  # insert file fragments into storage1
  id1=`insertFile $storageId1 "file1" $DATETIME1 1000   0 500`
  id2=`insertFile $storageId1 "file1" $DATETIME2 1000 500 500`

  # insert image fragments into storage1
  id3=`insertImage $storageId1 "image1" $DATETIME1 1000   0 500`
  id4=`insertImage $storageId1 "image1" $DATETIME2 1000 500 500`

  echo "--- before assign storage --------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id)                 FROM entries"                      4

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId1" 4
  verify "SELECT totalEntrySize            FROM storage WHERE id=$storageId1" 2000
  verify "SELECT totalEntryCountNewest     FROM storage WHERE id=$storageId1" 4
  verify "SELECT totalEntrySizeNewest      FROM storage WHERE id=$storageId1" 2000

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId2" 0
  verify "SELECT totalEntrySize            FROM storage WHERE id=$storageId2" 0
  verify "SELECT totalEntryCountNewest     FROM storage WHERE id=$storageId2" 0
  verify "SELECT totalEntrySizeNewest      FROM storage WHERE id=$storageId2" 0

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId1" 4

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2" 0

  verify "SELECT COUNT(id)                 FROM fileEntries"      2
  verify "SELECT COUNT(id)                 FROM imageEntries"     2
  verify "SELECT COUNT(id)                 FROM directoryEntries" 0
  verify "SELECT COUNT(id)                 FROM linkEntries"      0
  verify "SELECT COUNT(id)                 FROM hardlinkEntries"  0
  verify "SELECT COUNT(id)                 FROM specialEntries"   0

  verify "SELECT totalFileCount            FROM storage WHERE id=$storageId1" 2
  verify "SELECT totalFileSize             FROM storage WHERE id=$storageId1" 1000
  verify "SELECT totalFileCountNewest      FROM storage WHERE id=$storageId1" 2
  verify "SELECT totalFileSizeNewest       FROM storage WHERE id=$storageId1" 1000

  verify "SELECT totalImageCount           FROM storage WHERE id=$storageId1" 2
  verify "SELECT totalImageSize            FROM storage WHERE id=$storageId1" 1000
  verify "SELECT totalImageCountNewest     FROM storage WHERE id=$storageId1" 2
  verify "SELECT totalImageSizeNewest      FROM storage WHERE id=$storageId1" 1000

  verify "SELECT totalDirectoryCount       FROM storage WHERE id=$storageId1" 0
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId1" 0

  verify "SELECT totalLinkCount            FROM storage WHERE id=$storageId1" 0
  verify "SELECT totalLinkCountNewest      FROM storage WHERE id=$storageId1" 0

  verify "SELECT totalHardlinkCount        FROM storage WHERE id=$storageId1" 0
  verify "SELECT totalHardlinkSize         FROM storage WHERE id=$storageId1" 0
  verify "SELECT totalHardlinkCountNewest  FROM storage WHERE id=$storageId1" 0
  verify "SELECT totalHardlinkSizeNewest   FROM storage WHERE id=$storageId1" 0

  verify "SELECT totalSpecialCount         FROM storage WHERE id=$storageId1" 0
  verify "SELECT totalSpecialCountNewest   FROM storage WHERE id=$storageId1" 0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID1'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID1'" 4
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID1'" 2000

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID2'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID2'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID2'" 0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID3'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID3'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID3'" 0

  # assign entries of storage1 -> storage2
  $SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId2 WHERE id=$id1"
  $SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId2 WHERE id=$id2"
  $SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId2 WHERE id=$id3"
  $SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId2 WHERE id=$id4"

  echo "--- after assign storage ---------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id)                 FROM entries"                         4

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalEntrySize            FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalEntryCountNewest     FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalEntrySizeNewest      FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId1"    0
  verify "SELECT totalEntrySize            FROM storage WHERE id=$storageId2"    2000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2"    4
  verify "SELECT totalEntrySizeNewest      FROM storage WHERE id=$storageId2"    2000

  verify "SELECT COUNT(id)                 FROM fileEntries"                     2
  verify "SELECT COUNT(id)                 FROM imageEntries"                    2
  verify "SELECT COUNT(id)                 FROM directoryEntries"                0
  verify "SELECT COUNT(id)                 FROM linkEntries"                     0
  verify "SELECT COUNT(id)                 FROM hardlinkEntries"                 0
  verify "SELECT COUNT(id)                 FROM specialEntries"                  0

  verify "SELECT totalFileCount            FROM storage WHERE id=$storageId2"    2
  verify "SELECT totalFileSize             FROM storage WHERE id=$storageId2"    1000
  verify "SELECT totalFileCountNewest      FROM storage WHERE id=$storageId2"    2
  verify "SELECT totalFileSizeNewest       FROM storage WHERE id=$storageId2"    1000

  verify "SELECT totalImageCount           FROM storage WHERE id=$storageId2"    2
  verify "SELECT totalImageSize            FROM storage WHERE id=$storageId2"    1000
  verify "SELECT totalImageCountNewest     FROM storage WHERE id=$storageId2"    2
  verify "SELECT totalImageSizeNewest      FROM storage WHERE id=$storageId2"    1000

  verify "SELECT totalDirectoryCount       FROM storage WHERE id=$storageId2"    0
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId2"    0

  verify "SELECT totalLinkCount            FROM storage WHERE id=$storageId2"    0
  verify "SELECT totalLinkCountNewest      FROM storage WHERE id=$storageId2"    0

  verify "SELECT totalHardlinkCount        FROM storage WHERE id=$storageId2"    0
  verify "SELECT totalHardlinkSize         FROM storage WHERE id=$storageId2"    0
  verify "SELECT totalHardlinkCountNewest  FROM storage WHERE id=$storageId2"    0
  verify "SELECT totalHardlinkSizeNewest   FROM storage WHERE id=$storageId2"    0

  verify "SELECT totalSpecialCount         FROM storage WHERE id=$storageId2"    0
  verify "SELECT totalSpecialCountNewest   FROM storage WHERE id=$storageId2"    0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID1'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID1'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID1'" 0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID2'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID2'" 4
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID2'" 2000

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID3'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID3'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID3'" 0

  #---------------------------------------------------------------------

  # assign entity
  resetDatabase

  # insert file fragments into storage1
  id1=`insertFile $storageId1 "file1" $DATETIME1 1000   0 500`
  id2=`insertFile $storageId1 "file1" $DATETIME2 1000 500 500`

  # insert image fragments into storage1
  id3=`insertImage $storageId1 "image1" $DATETIME1 1000   0 500`
  id4=`insertImage $storageId1 "image1" $DATETIME2 1000 500 500`

  echo "--- before assign entity ---------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id)                 FROM entries"                         4

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId1"    4
  verify "SELECT totalEntrySize            FROM storage WHERE id=$storageId1"    2000
  verify "SELECT totalEntryCountNewest     FROM storage WHERE id=$storageId1"    4
  verify "SELECT totalEntrySizeNewest      FROM storage WHERE id=$storageId1"    2000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId1"    4
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId1"    2000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2"    0
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId2"    0

  verify "SELECT COUNT(id)                 FROM fileEntries"                     2
  verify "SELECT COUNT(id)                 FROM imageEntries"                    2
  verify "SELECT COUNT(id)                 FROM directoryEntries"                0
  verify "SELECT COUNT(id)                 FROM linkEntries"                     0
  verify "SELECT COUNT(id)                 FROM hardlinkEntries"                 0
  verify "SELECT COUNT(id)                 FROM specialEntries"                  0

  verify "SELECT totalFileCount            FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalFileSize             FROM storage WHERE id=$storageId1"    1000
  verify "SELECT totalFileCountNewest      FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalFileSizeNewest       FROM storage WHERE id=$storageId1"    1000

  verify "SELECT totalImageCount           FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalImageSize            FROM storage WHERE id=$storageId1"    1000
  verify "SELECT totalImageCountNewest     FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalImageSizeNewest      FROM storage WHERE id=$storageId1"    1000

  verify "SELECT totalDirectoryCount       FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalLinkCount            FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalLinkCountNewest      FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalHardlinkCount        FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkSize         FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkCountNewest  FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkSizeNewest   FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalSpecialCount         FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalSpecialCountNewest   FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID1'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID1'" 4
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID1'" 2000

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID2'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID2'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID2'" 0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID3'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID3'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID3'" 0

  # assign storage1 of entity1 -> entity2
  $SQLITE3 $databaseFile "UPDATE storage SET entityId=$entityId2 WHERE id=$storageId1"

  echo "--- after assign entity ----------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id)                 FROM entries"                         4

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId1"    4
  verify "SELECT totalEntrySize            FROM storage WHERE id=$storageId1"    2000
  verify "SELECT totalEntryCountNewest     FROM storage WHERE id=$storageId1"    4
  verify "SELECT totalEntrySizeNewest      FROM storage WHERE id=$storageId1"    2000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId1"    0
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId1"    0

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2"    4
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId2"    2000

  verify "SELECT COUNT(id)                 FROM fileEntries"                     2
  verify "SELECT COUNT(id)                 FROM imageEntries"                    2
  verify "SELECT COUNT(id)                 FROM directoryEntries"                0
  verify "SELECT COUNT(id)                 FROM linkEntries"                     0
  verify "SELECT COUNT(id)                 FROM hardlinkEntries"                 0
  verify "SELECT COUNT(id)                 FROM specialEntries"                  0

  verify "SELECT totalFileCount            FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalFileSize             FROM storage WHERE id=$storageId1"    1000
  verify "SELECT totalFileCountNewest      FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalFileSizeNewest       FROM storage WHERE id=$storageId1"    1000

  verify "SELECT totalImageCount           FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalImageSize            FROM storage WHERE id=$storageId1"    1000
  verify "SELECT totalImageCountNewest     FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalImageSizeNewest      FROM storage WHERE id=$storageId1"    1000

  verify "SELECT totalDirectoryCount       FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalLinkCount            FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalLinkCountNewest      FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalHardlinkCount        FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkSize         FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkCountNewest  FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkSizeNewest   FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalSpecialCount         FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalSpecialCountNewest   FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID1'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID1'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID1'" 0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID2'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID2'" 4
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID2'" 2000

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID3'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID3'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID3'" 0

  #---------------------------------------------------------------------

  # assign jobUUID
  resetDatabase

  # insert file fragments into storage1
  id1=`insertFile $storageId1 "file1" $DATETIME1 1000   0 500`
  id2=`insertFile $storageId1 "file1" $DATETIME2 1000 500 500`

  # insert image fragments into storage1
  id3=`insertImage $storageId1 "image1" $DATETIME1 1000   0 500`
  id4=`insertImage $storageId1 "image1" $DATETIME2 1000 500 500`

  echo "--- before assign jobUUID---------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id)                 FROM entries"                         4

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId1"    4
  verify "SELECT totalEntrySize            FROM storage WHERE id=$storageId1"    2000
  verify "SELECT totalEntryCountNewest     FROM storage WHERE id=$storageId1"    4
  verify "SELECT totalEntrySizeNewest      FROM storage WHERE id=$storageId1"    2000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId1"    4
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId1"    2000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2"    0
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId2"    0

  verify "SELECT COUNT(id)                 FROM fileEntries"                     2
  verify "SELECT COUNT(id)                 FROM imageEntries"                    2
  verify "SELECT COUNT(id)                 FROM directoryEntries"                0
  verify "SELECT COUNT(id)                 FROM linkEntries"                     0
  verify "SELECT COUNT(id)                 FROM hardlinkEntries"                 0
  verify "SELECT COUNT(id)                 FROM specialEntries"                  0

  verify "SELECT totalFileCount            FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalFileSize             FROM storage WHERE id=$storageId1"    1000
  verify "SELECT totalFileCountNewest      FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalFileSizeNewest       FROM storage WHERE id=$storageId1"    1000

  verify "SELECT totalImageCount           FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalImageSize            FROM storage WHERE id=$storageId1"    1000
  verify "SELECT totalImageCountNewest     FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalImageSizeNewest      FROM storage WHERE id=$storageId1"    1000

  verify "SELECT totalDirectoryCount       FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalLinkCount            FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalLinkCountNewest      FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalHardlinkCount        FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkSize         FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkCountNewest  FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkSizeNewest   FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalSpecialCount         FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalSpecialCountNewest   FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID1'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID1'" 4
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID1'" 2000

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID2'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID2'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID2'" 0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID3'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID3'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID3'" 0

  # assign entity1 of jobUUID1 -> jobUUID2
  $SQLITE3 $databaseFile "UPDATE entities SET jobUUID='$jobUUID2' WHERE jobUUID='$jobUUID1'"

  echo "--- after assign jobUUID ---------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT totalEntryCount           FROM storage WHERE id=$storageId1"    4
  verify "SELECT totalEntrySize            FROM storage WHERE id=$storageId1"    2000
  verify "SELECT totalEntryCountNewest     FROM storage WHERE id=$storageId1"    4
  verify "SELECT totalEntrySizeNewest      FROM storage WHERE id=$storageId1"    2000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId1"    4
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId1"    2000

  verify "SELECT totalEntryCount           FROM entities WHERE id=$entityId2"    0
  verify "SELECT totalEntrySize            FROM entities WHERE id=$entityId2"    0

  verify "SELECT COUNT(id)                 FROM fileEntries"                     2
  verify "SELECT COUNT(id)                 FROM imageEntries"                    2
  verify "SELECT COUNT(id)                 FROM directoryEntries"                0
  verify "SELECT COUNT(id)                 FROM linkEntries"                     0
  verify "SELECT COUNT(id)                 FROM hardlinkEntries"                 0
  verify "SELECT COUNT(id)                 FROM specialEntries"                  0

  verify "SELECT totalFileCount            FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalFileSize             FROM storage WHERE id=$storageId1"    1000
  verify "SELECT totalFileCountNewest      FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalFileSizeNewest       FROM storage WHERE id=$storageId1"    1000

  verify "SELECT totalImageCount           FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalImageSize            FROM storage WHERE id=$storageId1"    1000
  verify "SELECT totalImageCountNewest     FROM storage WHERE id=$storageId1"    2
  verify "SELECT totalImageSizeNewest      FROM storage WHERE id=$storageId1"    1000

  verify "SELECT totalDirectoryCount       FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalLinkCount            FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalLinkCountNewest      FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalHardlinkCount        FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkSize         FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkCountNewest  FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalHardlinkSizeNewest   FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalSpecialCount         FROM storage WHERE id=$storageId1"    0
  verify "SELECT totalSpecialCountNewest   FROM storage WHERE id=$storageId1"    0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID1'" 0
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID1'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID1'" 0

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID2'" 2
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID2'" 4
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID2'" 2000

  verify "SELECT totalEntityCount          FROM uuids WHERE jobUUID='$jobUUID3'" 1
  verify "SELECT totalEntryCount           FROM uuids WHERE jobUUID='$jobUUID3'" 0
  verify "SELECT totalEntrySize            FROM uuids WHERE jobUUID='$jobUUID3'" 0
fi

if test $testDirectory -eq 1; then
  # test directory
  resetDatabase

  id1=`insertDirectory $storageId1 "/d1"       $DATETIME1`
  id2=`insertDirectory $storageId1 "/d1/d2"    $DATETIME1`
  id3=`insertDirectory $storageId1 "/d1/d2/d3" $DATETIME1`

  id4=`insertFile $storageId1 "/d1/f1" $DATETIME1 1000   0 500`
  id5=`insertFile $storageId1 "/d1/f1" $DATETIME1 1000 500 500`
  id6=`insertFile $storageId1 "/d1/f1" $DATETIME2 1000 500 500`

  id7=`insertFile $storageId1 "/d1/d2/f2" $DATETIME1 1000   0 500`
  id8=`insertFile $storageId1 "/d1/d2/f2" $DATETIME1 1000 500 500`
  id9=`insertFile $storageId1 "/d1/d2/f2" $DATETIME2 1000 500 500`

  id10=`insertFile $storageId1 "/d1/d2/d3/f3" $DATETIME1 1000   0 500`
  id11=`insertFile $storageId1 "/d1/d2/d3/f3" $DATETIME1 1000 500 500`
  id12=`insertFile $storageId1 "/d1/d2/d3/f3" $DATETIME2 1000 500 500`

  echo "--- after insert into directories ------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id3" 3
  verify "SELECT totalEntrySize        FROM directoryEntries WHERE entryId=$id3" 1500
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id3" 2
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id3" 1000

  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id2" $((3+1+3))
  verify "SELECT totalEntrySize        FROM directoryEntries WHERE entryId=$id2" 3000
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id2" 5
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id2" 2000

  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id1" $((3+1+3+1+3))
  verify "SELECT totalEntrySize        FROM directoryEntries WHERE entryId=$id1" 4500
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id1" 8
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id1" 3000

  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id12"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id3" 2
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id3" 2
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id3" 1000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id11"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id3" 1
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id3" 1
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id3" 500
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id10"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id3" 0
  verify "SELECT totalEntrySize        FROM directoryEntries WHERE entryId=$id3" 0
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id3" 0
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id3" 0

  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id9"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id2" 3
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id2" 3
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id2" 1000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id8"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id2" 2
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id2" 2
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id2" 500
  $SQLITE3 $databaseFile "DELETE       FROM entries WHERE id=$id7"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id2" 1
  verify "SELECT totalEntrySize        FROM directoryEntries WHERE entryId=$id2" 0
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id2" 1
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id2" 0

  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id6"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id1" 4
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id1" 4
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id1" 1000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id5"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id1" 3
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id1" 3
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id1" 500
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id4"
  verify "SELECT totalEntryCount       FROM directoryEntries WHERE entryId=$id1" 2
  verify "SELECT totalEntrySize        FROM directoryEntries WHERE entryId=$id1" 0
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id1" 2
  verify "SELECT totalEntrySizeNewest  FROM directoryEntries WHERE entryId=$id1" 0

  echo "--- after delete from directories ------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"
fi

if test $testFTS -eq 1; then
  # test full-text-search
  resetDatabase

  insertFile $storageId2 "/d1/d2/test_123.abc" $DATETIME1 1000   0 1000 > /dev/null
  insertFile $storageId2 "/d3/d4/abc-123.xyz"  $DATETIME1 1000   0 1000 > /dev/null
  insertFile $storageId2 "a1b2c3d4"            $DATETIME1 1000   0 1000 > /dev/null
  insertFile $storageId2 "aabbccdd"            $DATETIME1 1000   0 1000 > /dev/null

  echo "--- before FTS match -------------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(entryId) FROM FTS_entries WHERE FTS_entries MATCH 'none'"      0
  verify "SELECT COUNT(entryId) FROM FTS_entries WHERE FTS_entries MATCH 'test*'"     1
  verify "SELECT COUNT(entryId) FROM FTS_entries WHERE FTS_entries MATCH '123'"       2
  verify "SELECT COUNT(entryId) FROM FTS_entries WHERE FTS_entries MATCH '12*'"       2
  verify "SELECT COUNT(entryId) FROM FTS_entries WHERE FTS_entries MATCH 'test* 12*'" 1
  verify "SELECT COUNT(entryId) FROM FTS_entries WHERE FTS_entries MATCH 'a1b2*'"     1
  verify "SELECT COUNT(entryId) FROM FTS_entries WHERE FTS_entries MATCH 'aabbcc*'"   1
fi

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#$SQLITE3 $databaseFile "SELECT rowid,* FROM log"
