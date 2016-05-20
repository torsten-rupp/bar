#!/bin/bash

SQLITE3=./bar-sqlite3
make $SQLITE3 > /dev/null

databaseFile=bar-index.db
testBase=0
testDelete=0
testAssign=1
testDirectory=0
testFTS=0

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
  echo "UUIDs:"
  $SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize,totalFileCount,totalFileSize FROM uuids"
  echo "Entities:"
  $SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
  $SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM entities"
  echo "Storage:"
  $SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
  $SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM storage"
  $SQLITE3 $databaseFile -H "SELECT id,totalFileCountNewest,totalFileSizeNewest,totalImageCountNewest,totalImageSizeNewest,totalDirectoryCountNewest,totalLinkCountNewest,totalHardlinkCountNewest,totalHardlinkSizeNewest,totalSpecialCountNewest FROM storage"
  echo "Entries:"
  $SQLITE3 $databaseFile -H "SELECT id,storageId,name,type,timeLastChanged,offset,size FROM entries"
  echo "Newest entries:"
  $SQLITE3 $databaseFile -H "SELECT id,entryId,storageId,entryId,name,type,offset,size FROM entriesNewest"
}

# create
$SQLITE3 $databaseFile -c

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# uuid
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('0000')"
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('1234')"
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('5678')"

# entity
entityId1=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('0000','',$DATETIME1,1); SELECT last_insert_rowid();"`
entityId2=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('1234','',$DATETIME1,1); SELECT last_insert_rowid();"`
entityId3=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('5678','',$DATETIME1,1); SELECT last_insert_rowid();"`
verify "SELECT COUNT(id) FROM entities" 4

# storage
storageId1=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId1,'s1',$DATETIME2,4400,1,1); SELECT last_insert_rowid();"`
storageId2=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId2,'s2',$DATETIME3,400,1,1); SELECT last_insert_rowid();"`
storageId3=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId3,'s3',$DATETIME4,0,1,1); SELECT last_insert_rowid();"`
verify "SELECT COUNT(id) FROM storage" 3

if test $testBase -eq 1; then
  # files
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storageTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storageTotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entitiesTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

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

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storageTotalEntryCount+6))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storageTotalEntryCountNewest+4))
    verify "SELECT totalFileCount FROM storage WHERE id=$storageId1" 6
    verify "SELECT totalFileSize FROM storage WHERE id=$storageId1" 3000
    verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId1" 4
    verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId1" 2000

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entitiesTotalEntryCount+6))
    verify "SELECT totalFileCount FROM entities WHERE id=$entityId1" 6
    verify "SELECT totalFileSize FROM entities WHERE id=$entityId1" 3000
  fi

  # images
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storageTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storageTotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entitiesTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

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

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storageTotalEntryCount+6))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storageTotalEntryCountNewest+4))
    verify "SELECT totalImageCount FROM storage WHERE id=$storageId1" 6
    verify "SELECT totalImageSize FROM storage WHERE id=$storageId1" 3000
    verify "SELECT totalImageCountNewest FROM storage WHERE id=$storageId1" 4
    verify "SELECT totalImageSizeNewest FROM storage WHERE id=$storageId1" 2000

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entitiesTotalEntryCount+6))
    verify "SELECT totalImageCount FROM entities WHERE id=$entityId1" 6
    verify "SELECT totalImageSize FROM entities WHERE id=$entityId1" 3000
  fi

  # directories
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storageTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storageTotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entitiesTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir1')"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir2')"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir2')"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+3))
    verify "SELECT COUNT(id) FROM directoryEntries" 3;

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storageTotalEntryCount+3))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storageTotalEntryCountNewest+2))
    verify "SELECT totalDirectoryCount FROM storage WHERE id=$storageId1" 3
    verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId1" 2

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entitiesTotalEntryCount+3))
    verify "SELECT totalDirectoryCount FROM entities WHERE id=$entityId1" 3
  fi

  # links
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storageTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storageTotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entitiesTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination1')"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination2')"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination2')"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+3));
    verify "SELECT COUNT(id) FROM linkEntries" 3;

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storageTotalEntryCount+3))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storageTotalEntryCountNewest+2))
    verify "SELECT totalLinkCount FROM storage WHERE id=$storageId1" 3
    verify "SELECT totalLinkCountNewest FROM storage WHERE id=$storageId1" 2

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entitiesTotalEntryCount+3))
    verify "SELECT totalLinkCount FROM entities WHERE id=$entityId1" 3
  fi

  # hardlink
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storageTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storageTotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entitiesTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

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

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storageTotalEntryCount+6))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storageTotalEntryCountNewest+4))
    verify "SELECT totalHardlinkCount FROM storage WHERE id=$storageId1" 6
    verify "SELECT totalHardlinkSize FROM storage WHERE id=$storageId1" 3000
    verify "SELECT totalHardlinkCountNewest FROM storage WHERE id=$storageId1" 4
    verify "SELECT totalHardlinkSizeNewest FROM storage WHERE id=$storageId1" 2000

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entitiesTotalEntryCount+6))
    verify "SELECT totalHardlinkCount FROM entities WHERE id=$entityId1" 6
  fi

  # special
  if true; then
    entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
    storageTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
    storageTotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1"`
    entitiesTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s1',$DATETIME1); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,1)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s2',$DATETIME2); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,2)"

    id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s2',$DATETIME3); SELECT last_insert_rowid();"`
    $SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,2)"

    verify "SELECT COUNT(id) FROM entries" $(($entriesCount+3))
    verify "SELECT COUNT(id) FROM specialEntries" 3

    verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($storageTotalEntryCount+3))
    verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId1" $(($storageTotalEntryCountNewest+2))
    verify "SELECT totalSpecialCount FROM storage WHERE id=$storageId1" 3
    verify "SELECT totalSpecialCountNewest FROM storage WHERE id=$storageId1" 2

    verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($entitiesTotalEntryCount+3))
    verify "SELECT totalSpecialCount FROM entities WHERE id=$entityId1" 3
  fi

  printValues
  echo "----------------------------------------------------------------------"
fi

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if test $testDelete -eq 1; then
  # insert+delete
  entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
  storageTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId2"`
  storageTotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId2"`
  entitiesTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId2"`

  fileEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM fileEntries"`
  imageEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM imageEntries"`
  directoryEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM directoryEntries"`
  linkEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM linkEntries"`
  hardlinkEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM hardlinkEntries"`
  specialEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM specialEntries"`

  # insert 2 file fragments
  id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'xf1',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id,1000,0,500);"
  id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'xf1',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id,1000,500,500);"
  # insert 2 file fragments with newer date
  id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'xf1',$DATETIME2); SELECT last_insert_rowid();"`
  id2=`$SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id1,1000,0,500); SELECT last_insert_rowid();"`
  id3=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'xf1',$DATETIME2); SELECT last_insert_rowid();"`
  id4=`$SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id3,1000,500,500); SELECT last_insert_rowid();"`

  id5=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_IMAGE,'xi1',$DATETIME1); SELECT last_insert_rowid();"`
  id6=`$SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId2,$id5,1000,1,0,500); SELECT last_insert_rowid();"`

  id7=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_DIRECTORY,'xd1',$DATETIME1); SELECT last_insert_rowid();"`
  id8=`$SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId2,$id7,$storageId2,'dir1'); SELECT last_insert_rowid();"`

  id9=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_LINK,'xl1',$DATETIME1); SELECT last_insert_rowid();"`
  id10=`$SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId2,$id9,'destination1'); SELECT last_insert_rowid();"`

  id11=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_HARDLINK,'xh1',$DATETIME1); SELECT last_insert_rowid();"`
  id12=`$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id11,1000,0,500); SELECT last_insert_rowid();"`

  id13=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_SPECIAL,'xs1',$DATETIME1); SELECT last_insert_rowid();"`
  id14=`$SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId2,$id13,1); SELECT last_insert_rowid();"`

  echo "--- after insert -----------------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id) FROM entries" $(($entriesCount+4+1+1+1+1+1))
  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" $(($storageTotalEntryCount+4+1+1+1+1+1))
  verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId2" $((storageTotalEntryCountNewest+2+1+1+1+1+1))
  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" $(($entitiesTotalEntryCount+4+1+1+1+1+1))

  verify "SELECT COUNT(id) FROM fileEntries" $(($fileEntriesCount+4))
  verify "SELECT COUNT(id) FROM imageEntries" $(($imageEntriesCount+1))
  verify "SELECT COUNT(id) FROM directoryEntries" $(($directoryEntriesCount+1))
  verify "SELECT COUNT(id) FROM linkEntries" $(($linkEntriesCount+1))
  verify "SELECT COUNT(id) FROM hardlinkEntries" $(($hardlinkEntriesCount+1))
  verify "SELECT COUNT(id) FROM specialEntries" $(($specialEntriesCount+1))

  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" 9

  verify "SELECT totalFileCount FROM storage WHERE id=$storageId2" 4
  verify "SELECT totalFileSize FROM storage WHERE id=$storageId2" 2000
  verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId2" 2
  verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId2" 1000

  verify "SELECT totalImageCount FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalImageSize FROM storage WHERE id=$storageId2" 500
  verify "SELECT totalImageCountNewest FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalImageSizeNewest FROM storage WHERE id=$storageId2" 500

  verify "SELECT totalDirectoryCount FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId2" 1

  verify "SELECT totalLinkCount FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalLinkCountNewest FROM storage WHERE id=$storageId2" 1

  verify "SELECT totalHardlinkCount FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalHardlinkSize FROM storage WHERE id=$storageId2" 500
  verify "SELECT totalHardlinkCountNewest FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalHardlinkSizeNewest FROM storage WHERE id=$storageId2" 500

  verify "SELECT totalSpecialCount FROM storage WHERE id=$storageId2" 1
  verify "SELECT totalSpecialCountNewest FROM storage WHERE id=$storageId2" 1

  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" 9
  verify "SELECT totalFileCount FROM entities WHERE id=$entityId2" 4
  verify "SELECT totalFileSize FROM entities WHERE id=$entityId2" 2000

  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id13"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id11"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id9"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id7"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id5"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id3"
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id1"

  echo "--- after delete -----------------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id) FROM entries" $(($entriesCount+2));
  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" $(($storageTotalEntryCount+2))
  verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId2" $((storageTotalEntryCountNewest+2))
  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" $(($entitiesTotalEntryCount+2))

  verify "SELECT COUNT(id) FROM fileEntries" $(($fileEntriesCount+2))
  verify "SELECT COUNT(id) FROM imageEntries" $(($imageEntriesCount+0))
  verify "SELECT COUNT(id) FROM directoryEntries" $(($directoryEntriesCount+0))
  verify "SELECT COUNT(id) FROM linkEntries" $(($linkEntriesCount+0))
  verify "SELECT COUNT(id) FROM hardlinkEntries" $(($hardlinkEntriesCount+0))
  verify "SELECT COUNT(id) FROM specialEntries" $(($specialEntriesCount+0))

  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" 2
  verify "SELECT totalFileCount FROM storage WHERE id=$storageId2" 2
  verify "SELECT totalFileSize FROM storage WHERE id=$storageId2" 1000
  verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId2" 2
  verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId2" 1000

  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" 2
  verify "SELECT totalFileCount FROM entities WHERE id=$entityId2" 2
  verify "SELECT totalFileSize FROM entities WHERE id=$entityId2" 1000
fi

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if test $testAssign -eq 1; then
  # assign
  entriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
  storageTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId2"`
  storageTotalEntrySize=`$SQLITE3 $databaseFile "SELECT totalEntrySize FROM storage WHERE id=$storageId2"`
  storageTotalEntryCountNewest=`$SQLITE3 $databaseFile "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId2"`
  storageTotalEntrySizeNewest=`$SQLITE3 $databaseFile "SELECT totalEntrySizeNewest FROM storage WHERE id=$storageId2"`
  entitiesTotalEntryCount=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId2"`

  storageTotalFileCount=`$SQLITE3 $databaseFile "SELECT totalFileCount FROM storage WHERE id=$storageId2"`
  storageTotalFileSize=`$SQLITE3 $databaseFile "SELECT totalFileSize FROM storage WHERE id=$storageId2"`
  storageTotalFileCountNewest=`$SQLITE3 $databaseFile "SELECT totalFileCountNewest FROM storage WHERE id=$storageId2"`
  storageTotalFileSizeNewest=`$SQLITE3 $databaseFile "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId2"`

  storageTotalImageCount=`$SQLITE3 $databaseFile "SELECT totalImageCount FROM storage WHERE id=$storageId2"`
  storageTotalImageSize=`$SQLITE3 $databaseFile "SELECT totalImageSize FROM storage WHERE id=$storageId2"`
  storageTotalImageCountNewest=`$SQLITE3 $databaseFile "SELECT totalImageCountNewest FROM storage WHERE id=$storageId2"`
  storageTotalImageSizeNewest=`$SQLITE3 $databaseFile "SELECT totalImageSizeNewest FROM storage WHERE id=$storageId2"`

  storageTotalDirectoryCount=`$SQLITE3 $databaseFile "SELECT totalDirectoryCount FROM storage WHERE id=$storageId2"`
  storageTotalDirectoryCountNewest=`$SQLITE3 $databaseFile "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId2"`

  storageTotalLinkCount=`$SQLITE3 $databaseFile "SELECT totalLinkCount FROM storage WHERE id=$storageId2"`
  storageTotalLinkCountNewest=`$SQLITE3 $databaseFile "SELECT totalLinkCountNewest FROM storage WHERE id=$storageId2"`

  storageTotalHardlinkCount=`$SQLITE3 $databaseFile "SELECT totalHardlinkCount FROM storage WHERE id=$storageId2"`
  storageTotalHardlinkSize=`$SQLITE3 $databaseFile "SELECT totalHardlinkSize FROM storage WHERE id=$storageId2"`
  storageTotalHardlinkCountNewest=`$SQLITE3 $databaseFile "SELECT totalHardlinkCountNewest FROM storage WHERE id=$storageId2"`
  storageTotalHardlinkSizeNewest=`$SQLITE3 $databaseFile "SELECT totalHardlinkSizeNewest FROM storage WHERE id=$storageId2"`

  storageTotalSpecialCount=`$SQLITE3 $databaseFile "SELECT totalSpecialCount FROM storage WHERE id=$storageId2"`
  storageTotalSpecialCountNewest=`$SQLITE3 $databaseFile "SELECT totalSpecialCountNewest FROM storage WHERE id=$storageId2"`

  fileEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM fileEntries"`
  imageEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM imageEntries"`
  directoryEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM directoryEntries"`
  linkEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM linkEntries"`
  hardlinkEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM hardlinkEntries"`
  specialEntriesCount=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM specialEntries"`

  # insert 1 file fragment
  id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'af1',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id1,1000,0,500)"
  # insert 1 file fragment with newer date
  id2=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'af1',$DATETIME2); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id2,1000,0,500)"

  # insert 1 image fragment
  id3=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_IMAGE,'ai1',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId2,$id3,1000,1,0,500)"
  # insert 1 image fragment with newer date
  id4=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_IMAGE,'ai1',$DATETIME2); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId2,$id4,1000,1,0,500)"

  verify "SELECT COUNT(id) FROM entries" $(($entriesCount+4))
  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" $(($storageTotalEntryCount+4))
  verify "SELECT totalEntrySize FROM storage WHERE id=$storageId2" $(($storageTotalEntrySize+2000))
  verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId2" $(($storageTotalEntryCountNewest+2))
  verify "SELECT totalEntrySizeNewest FROM storage WHERE id=$storageId2" $(($storageTotalEntrySizeNewest+1000))
  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" $(($entitiesTotalEntryCount+4))

  echo "--- before assign ----------------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id) FROM entries" $(($entriesCount+4))
  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" $(($storageTotalEntryCount+4))
  verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId2" $((storageTotalEntryCountNewest+2))
  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId3" 0
  verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId3" 0
  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" $(($entitiesTotalEntryCount+4))
  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId3" 0

  verify "SELECT COUNT(id) FROM fileEntries" $(($fileEntriesCount+2))
  verify "SELECT COUNT(id) FROM imageEntries" $(($imageEntriesCount+2))
  verify "SELECT COUNT(id) FROM directoryEntries" $(($directoryEntriesCount+0))
  verify "SELECT COUNT(id) FROM linkEntries" $(($linkEntriesCount+0))
  verify "SELECT COUNT(id) FROM hardlinkEntries" $(($hardlinkEntriesCount+0))
  verify "SELECT COUNT(id) FROM specialEntries" $(($specialEntriesCount+0))

  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" $(($storageTotalEntryCount+4))

  verify "SELECT totalFileCount FROM storage WHERE id=$storageId2" $(($storageTotalFileCount+2))
  verify "SELECT totalFileSize FROM storage WHERE id=$storageId2" $(($storageTotalFileSize+1000))
  verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId2" $(($storageTotalFileCountNewest+1))
  verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId2" $(($storageTotalFileSizeNewest+500))

  verify "SELECT totalImageCount FROM storage WHERE id=$storageId2" $(($storageTotalImageCount+2))
  verify "SELECT totalImageSize FROM storage WHERE id=$storageId2" $(($storageTotalImageSize+1000))
  verify "SELECT totalImageCountNewest FROM storage WHERE id=$storageId2" $(($storageTotalImageCountNewest+1))
  verify "SELECT totalImageSizeNewest FROM storage WHERE id=$storageId2" $(($storageTotalImageSizeNewest+500))

  verify "SELECT totalDirectoryCount FROM storage WHERE id=$storageId2" $(($storageTotalDirectoryCount+0))
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId2" $(($storageTotalDirectoryCountNewest+0))

  verify "SELECT totalLinkCount FROM storage WHERE id=$storageId2" $(($storageTotalLinkCount+0))
  verify "SELECT totalLinkCountNewest FROM storage WHERE id=$storageId2" $(($storageTotalLinkCountNewest+0))

  verify "SELECT totalHardlinkCount FROM storage WHERE id=$storageId2" $(($storageTotalHardlinkCount+0))
  verify "SELECT totalHardlinkSize FROM storage WHERE id=$storageId2" $(($storageTotalHardlinkSize+0))
  verify "SELECT totalHardlinkCountNewest FROM storage WHERE id=$storageId2" $(($storageTotalHardlinkCountNewest+0))
  verify "SELECT totalHardlinkSizeNewest FROM storage WHERE id=$storageId2" $(($storageTotalHardlinkSizeNewest+0))

  verify "SELECT totalSpecialCount FROM storage WHERE id=$storageId2" $(($storageTotalSpecialCount+0))
  verify "SELECT totalSpecialCountNewest FROM storage WHERE id=$storageId2" $(($storageTotalSpecialCountNewest+0))

  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" $(($entitiesTotalEntryCount+4))

  $SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId3 WHERE id=$id4"
  $SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId3 WHERE id=$id3"
  $SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId3 WHERE id=$id2"
  $SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId3 WHERE id=$id1"

  echo "--- after assign -----------------------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT COUNT(id) FROM entries" $(($entriesCount+4))
  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" $storageTotalEntryCount
  verify "SELECT totalEntrySize FROM storage WHERE id=$storageId2" $storageTotalEntrySize
  verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId2" $storageTotalEntryCountNewest
  verify "SELECT totalEntrySizeNewest FROM storage WHERE id=$storageId2" $storageTotalEntrySizeNewest
  verify "SELECT totalEntryCount FROM storage WHERE id=$storageId3" 4
  verify "SELECT totalEntrySize FROM storage WHERE id=$storageId3" 2000
  verify "SELECT totalEntryCountNewest FROM storage WHERE id=$storageId3" 2
  verify "SELECT totalEntrySizeNewest FROM storage WHERE id=$storageId3" 1000
  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" $entitiesTotalEntryCount
  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId3" 4

  verify "SELECT COUNT(id) FROM fileEntries" $(($fileEntriesCount+2))
  verify "SELECT COUNT(id) FROM imageEntries" $(($imageEntriesCount+2))
  verify "SELECT COUNT(id) FROM directoryEntries" $(($directoryEntriesCount+0))
  verify "SELECT COUNT(id) FROM linkEntries" $(($linkEntriesCount+0))
  verify "SELECT COUNT(id) FROM hardlinkEntries" $(($hardlinkEntriesCount+0))
  verify "SELECT COUNT(id) FROM specialEntries" $(($specialEntriesCount+0))

  verify "SELECT totalFileCount FROM storage WHERE id=$storageId2" $storageTotalFileCount
  verify "SELECT totalFileSize FROM storage WHERE id=$storageId2" $storageTotalFileSize
  verify "SELECT totalFileCount FROM storage WHERE id=$storageId3" 2
  verify "SELECT totalFileSize FROM storage WHERE id=$storageId3" 1000
  verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId3" 1
  verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId3" 500

  verify "SELECT totalImageCount FROM storage WHERE id=$storageId2" $storageTotalImageCount
  verify "SELECT totalImageSize FROM storage WHERE id=$storageId2" $storageTotalImageSize
  verify "SELECT totalImageCount FROM storage WHERE id=$storageId3" 2
  verify "SELECT totalImageSize FROM storage WHERE id=$storageId3" 1000
  verify "SELECT totalImageCountNewest FROM storage WHERE id=$storageId3" 1
  verify "SELECT totalImageSizeNewest FROM storage WHERE id=$storageId3" 500

  verify "SELECT totalDirectoryCount FROM storage WHERE id=$storageId3" 0
  verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId3" 0

  verify "SELECT totalLinkCount FROM storage WHERE id=$storageId3" 0
  verify "SELECT totalLinkCountNewest FROM storage WHERE id=$storageId3" 0

  verify "SELECT totalHardlinkCount FROM storage WHERE id=$storageId3" 0
  verify "SELECT totalHardlinkSize FROM storage WHERE id=$storageId3" 0
  verify "SELECT totalHardlinkCountNewest FROM storage WHERE id=$storageId3" 0
  verify "SELECT totalHardlinkSizeNewest FROM storage WHERE id=$storageId3" 0

  verify "SELECT totalSpecialCount FROM storage WHERE id=$storageId3" 0
  verify "SELECT totalSpecialCountNewest FROM storage WHERE id=$storageId3" 0

  verify "SELECT totalEntryCount FROM entities WHERE id=$entityId3" 4
  verify "SELECT totalFileCount FROM entities WHERE id=$entityId3" 2
  verify "SELECT totalFileSize FROM entities WHERE id=$entityId3" 1000
fi

if test $testDirectory -eq 1; then
  # test directory
  id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'/d1',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id1,$storageId1,'/d1')"
  id2=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'/d1/d2',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id2,$storageId1,'/d1/d2')"
  id3=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'/d1/d2/d3',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id3,$storageId1,'/d1/d2/d3')"

  id4=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/f1',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id4,2000,0,1000)"
  id5=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/f1',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id5,2000,1000,1000)"
  id6=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/f1',$DATETIME2); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id6,2000,1000,1000)"

  id7=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/d2/f2',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id7,2000,0,1000)"
  id8=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/d2/f2',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id8,2000,1000,1000)"
  id9=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/d2/f2',$DATETIME2); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id9,2000,1000,1000)"

  id10=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/d2/d3/f3',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id10,2000,0,1000)"
  id11=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/d2/d3/f3',$DATETIME1); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id11,2000,1000,1000)"
  id12=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/d2/d3/f3',$DATETIME2); SELECT last_insert_rowid();"`
  $SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId1,$id12,2000,1000,1000)"

  echo "--- after insert into directories ------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"

  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id3" 3
  verify "SELECT totalEntrySize FROM directoryEntries WHERE entryId=$id3" 3000
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id3" $((1+1))
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id3" 2000

  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id2" $((3+1+3))
  verify "SELECT totalEntrySize FROM directoryEntries WHERE entryId=$id2" 6000
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id2" $((2+1+2))
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id2" 4000

  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id1" $((3+1+3+1+3))
  verify "SELECT totalEntrySize FROM directoryEntries WHERE entryId=$id1" 9000
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id1" $((2+1+2+1+2))
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id1" 6000

  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id12"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id3" 2
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id3" 2
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id3" 2000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id11"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id3" 1
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id3" 1
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id3" 1000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id10"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id3" 0
  verify "SELECT totalEntrySize FROM directoryEntries WHERE entryId=$id3" 0
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id3" 0
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id3" 0

  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id9"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id2" 3
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id2" 3
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id2" 2000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id8"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id2" 2
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id2" 2
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id2" 1000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id7"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id2" 1
  verify "SELECT totalEntrySize FROM directoryEntries WHERE entryId=$id2" 0
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id2" 1
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id2" 0

  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id6"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id1" 4
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id1" 4
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id1" 2000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id5"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id1" 3
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id1" 3
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id1" 1000
  $SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id4"
  verify "SELECT totalEntryCount FROM directoryEntries WHERE entryId=$id1" 2
  verify "SELECT totalEntrySize FROM directoryEntries WHERE entryId=$id1" 0
  verify "SELECT totalEntryCountNewest FROM directoryEntries WHERE entryId=$id1" 2
  verify "SELECT totalEntrySizeNewest FROM directoryEntries WHERE entryId=$id1" 00

  echo "--- after delete from directories ------------------------------------"
  printValues
  echo "----------------------------------------------------------------------"
fi

if test $testFTS -eq 1; then
  # test full-text-search
  $SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d1/d2/test_123.abc',$DATETIME1);"
  $SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'/d3/d4/abc-123.xyz',$DATETIME1);"
  $SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'a1b2c3d4',$DATETIME1);"
  $SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'aabbccdd',$DATETIME1);"

#  $SQLITE3 $databaseFile "SELECT entryId,name FROM FTS_entries WHERE FTS_entries MATCH 'none'"
#  $SQLITE3 $databaseFile "SELECT entryId,name FROM FTS_entries WHERE FTS_entries MATCH 'test*'"
  $SQLITE3 $databaseFile "SELECT entryId,name FROM FTS_entries WHERE FTS_entries MATCH '123'"
#  $SQLITE3 $databaseFile "SELECT entryId,name FROM FTS_entries WHERE FTS_entries MATCH '12*'"
#  $SQLITE3 $databaseFile "SELECT entryId,name FROM FTS_entries WHERE FTS_entries MATCH 'test* 12*'"
  $SQLITE3 $databaseFile "SELECT entryId,name FROM FTS_entries WHERE FTS_entries MATCH 'a1b2*'"
  $SQLITE3 $databaseFile "SELECT entryId,name FROM FTS_entries WHERE FTS_entries MATCH 'aabbcc*'"
fi

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#$SQLITE3 $databaseFile "SELECT rowid,* FROM log"
