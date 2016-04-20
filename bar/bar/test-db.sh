#!/bin/bash

SQLITE3=./bar-sqlite3
make $SQLITE3 > /dev/null

databaseFile=bar-index.db

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
  if test "$value" -ne "$expected"; then
    echo >&2 "ERROR at `caller`: expected $expected, got $value: $sqlCommand"
    exit 1
  fi
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
verify "SELECT COUNT(id) FROM entities" 4;

# storage
storageId1=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId1,'s1',$DATETIME2,4400,1,1); SELECT last_insert_rowid();"`
storageId2=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId2,'s2',$DATETIME3,400,1,1); SELECT last_insert_rowid();"`
storageId3=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId3,'s3',$DATETIME4,0,1,1); SELECT last_insert_rowid();"`
verify "SELECT COUNT(id) FROM storage" 3;

if false; then
# files
if true; then
n1=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
n2=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
n3=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

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

verify "SELECT COUNT(id) FROM entries" $(($n1+6));
verify "SELECT COUNT(id) FROM fileEntries" 6;

verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($n2+6));
verify "SELECT totalFileCount FROM storage WHERE id=$storageId1" 6;
verify "SELECT totalFileSize FROM storage WHERE id=$storageId1" 3000;
verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId1" 4;
verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId1" 2000;

verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($n3+6));
verify "SELECT totalFileCount FROM entities WHERE id=$entityId1" 6;
verify "SELECT totalFileSize FROM entities WHERE id=$entityId1" 3000;
fi

# images
if true; then
n1=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
n2=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
n3=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

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

verify "SELECT COUNT(id) FROM entries" $(($n1+6));
verify "SELECT COUNT(id) FROM imageEntries" 6;

verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($n2+6));
verify "SELECT totalImageCount FROM storage WHERE id=$storageId1" 6;
verify "SELECT totalImageSize FROM storage WHERE id=$storageId1" 3000;
verify "SELECT totalImageCountNewest FROM storage WHERE id=$storageId1" 4;
verify "SELECT totalImageSizeNewest FROM storage WHERE id=$storageId1" 2000;

verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($n3+6));
verify "SELECT totalImageCount FROM entities WHERE id=$entityId1" 6;
verify "SELECT totalImageSize FROM entities WHERE id=$entityId1" 3000;
fi

# directories
if true; then
n1=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
n2=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
n3=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir1')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir2')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId1,$id,$storageId1,'dir2')"

verify "SELECT COUNT(id) FROM entries" $(($n1+3));
verify "SELECT COUNT(id) FROM directoryEntries" 3;

verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($n2+3));
verify "SELECT totalDirectoryCount FROM storage WHERE id=$storageId1" 3;
verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId1" 2;

verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($n3+3));
verify "SELECT totalDirectoryCount FROM entities WHERE id=$entityId1" 3;
fi

# links
if true; then
n1=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
n2=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
n3=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination1')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination2')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId1,$id,'destination2')"

verify "SELECT COUNT(id) FROM entries" $(($n1+3));
verify "SELECT COUNT(id) FROM linkEntries" 3;

verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($n2+3));
verify "SELECT totalLinkCount FROM storage WHERE id=$storageId1" 3;
verify "SELECT totalLinkCountNewest FROM storage WHERE id=$storageId1" 2;

verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($n3+3));
verify "SELECT totalLinkCount FROM entities WHERE id=$entityId1" 3;
fi

# hardlink
if true; then
n1=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
n2=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
n3=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

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

verify "SELECT COUNT(id) FROM entries" $(($n1+6));
verify "SELECT COUNT(id) FROM hardlinkEntries" 6;

verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($n2+6));
verify "SELECT totalHardlinkCount FROM storage WHERE id=$storageId1" 6;
verify "SELECT totalHardlinkSize FROM storage WHERE id=$storageId1" 3000;
verify "SELECT totalHardlinkCountNewest FROM storage WHERE id=$storageId1" 4;
verify "SELECT totalHardlinkSizeNewest FROM storage WHERE id=$storageId1" 2000;

verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($n3+6));
verify "SELECT totalHardlinkCount FROM entities WHERE id=$entityId1" 6;
fi

# special
if true; then
n1=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
n2=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId1"`
n3=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId1"`

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,1)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,2)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId1,$id,2)"

verify "SELECT COUNT(id) FROM entries" $(($n1+3));
verify "SELECT COUNT(id) FROM specialEntries" 3;

verify "SELECT totalEntryCount FROM storage WHERE id=$storageId1" $(($n2+3));
verify "SELECT totalSpecialCount FROM storage WHERE id=$storageId1" 3;
verify "SELECT totalSpecialCountNewest FROM storage WHERE id=$storageId1" 2;

verify "SELECT totalEntryCount FROM entities WHERE id=$entityId1" $(($n3+3));
verify "SELECT totalSpecialCount FROM entities WHERE id=$entityId1" 3;
fi

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
echo "----------------------------------------------------------------------"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
fi

if true; then
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# insert+delete
n1=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM entries"`
n2=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM storage WHERE id=$storageId2"`
n3=`$SQLITE3 $databaseFile "SELECT totalEntryCount FROM entities WHERE id=$entityId2"`

n4=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM fileEntries"`
n5=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM imageEntries"`
n6=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM directoryEntries"`
n7=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM linkEntries"`
n8=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM hardlinkEntries"`
n9=`$SQLITE3 $databaseFile "SELECT COUNT(id) FROM specialEntries"`

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'xf1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id,1000,0,500);"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'xf1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id,1000,500,500);"
id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'xf1',$DATETIME2); SELECT last_insert_rowid();"`
id2=`$SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id1,1000,0,500); SELECT last_insert_rowid();"`
id3=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'xf1',$DATETIME2); SELECT last_insert_rowid();"`
id4=`$SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id3,1000,500,500); SELECT last_insert_rowid();"`

id5=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_IMAGE,'i1',$DATETIME1); SELECT last_insert_rowid();"`
id6=`$SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId2,$id5,1000,1,0,500); SELECT last_insert_rowid();"`

id7=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_DIRECTORY,'d1',$DATETIME1); SELECT last_insert_rowid();"`
id8=`$SQLITE3 $databaseFile "INSERT INTO directoryEntries (storageId,entryId,storageId,name) VALUES ($storageId2,$id7,$storageId2,'dir1'); SELECT last_insert_rowid();"`

id9=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_LINK,'l1',$DATETIME1); SELECT last_insert_rowid();"`
id10=`$SQLITE3 $databaseFile "INSERT INTO linkEntries (storageId,entryId,destinationName) VALUES ($storageId2,$id9,'destination1'); SELECT last_insert_rowid();"`

id11=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_HARDLINK,'h1',$DATETIME1); SELECT last_insert_rowid();"`
id12=`$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id11,1000,0,500); SELECT last_insert_rowid();"`

id13=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_SPECIAL,'xs1',$DATETIME1); SELECT last_insert_rowid();"`
id14=`$SQLITE3 $databaseFile "INSERT INTO specialEntries (storageId,entryId,specialType) VALUES ($storageId2,$id13,1); SELECT last_insert_rowid();"`

verify "SELECT COUNT(id) FROM entries" $(($n1+4+1+1+1+1+1));
verify "SELECT COUNT(id) FROM fileEntries" $(($n4+4));
verify "SELECT COUNT(id) FROM imageEntries" $(($n5+1));
verify "SELECT COUNT(id) FROM directoryEntries" $(($n6+1));
verify "SELECT COUNT(id) FROM linkEntries" $(($n7+1));
verify "SELECT COUNT(id) FROM hardlinkEntries" $(($n8+1));
verify "SELECT COUNT(id) FROM specialEntries" $(($n9+1));

verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" 9;

verify "SELECT totalFileCount FROM storage WHERE id=$storageId2" 4;
verify "SELECT totalFileSize FROM storage WHERE id=$storageId2" 2000;
verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId2" 2;
verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId2" 1000;

verify "SELECT totalImageCount FROM storage WHERE id=$storageId2" 1;
verify "SELECT totalImageSize FROM storage WHERE id=$storageId2" 500;
verify "SELECT totalImageCountNewest FROM storage WHERE id=$storageId2" 1;
verify "SELECT totalImageSizeNewest FROM storage WHERE id=$storageId2" 500;

verify "SELECT totalDirectoryCount FROM storage WHERE id=$storageId2" 1;
verify "SELECT totalDirectoryCountNewest FROM storage WHERE id=$storageId2" 1;

verify "SELECT totalLinkCount FROM storage WHERE id=$storageId2" 1;
verify "SELECT totalLinkCountNewest FROM storage WHERE id=$storageId2" 1;

verify "SELECT totalHardlinkCount FROM storage WHERE id=$storageId2" 1;
verify "SELECT totalHardlinkSize FROM storage WHERE id=$storageId2" 500;
verify "SELECT totalHardlinkCountNewest FROM storage WHERE id=$storageId2" 1;
verify "SELECT totalHardlinkSizeNewest FROM storage WHERE id=$storageId2" 500;

verify "SELECT totalSpecialCount FROM storage WHERE id=$storageId2" 1;
verify "SELECT totalSpecialCountNewest FROM storage WHERE id=$storageId2" 1;

verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" 9;
verify "SELECT totalFileCount FROM entities WHERE id=$entityId2" 4;
verify "SELECT totalFileSize FROM entities WHERE id=$entityId2" 2000;

echo "--- after insert -----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
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
echo "----------------------------------------------------------------------"

$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id13"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id11"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id9"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id7"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id5"
#$SQLITE3 $databaseFile "DELETE FROM fileEntries WHERE id=$id4"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id3"
#$SQLITE3 $databaseFile "DELETE FROM fileEntries WHERE id=$id2"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id1"

verify "SELECT COUNT(id) FROM entries" $(($n1+2));
verify "SELECT COUNT(id) FROM fileEntries" $(($n4+2));
verify "SELECT COUNT(id) FROM imageEntries" $n5;
verify "SELECT COUNT(id) FROM directoryEntries" $n6;
verify "SELECT COUNT(id) FROM linkEntries" $n7;
verify "SELECT COUNT(id) FROM hardlinkEntries" $n8;
verify "SELECT COUNT(id) FROM specialEntries" $n9;

verify "SELECT totalEntryCount FROM storage WHERE id=$storageId2" 2;
verify "SELECT totalFileCount FROM storage WHERE id=$storageId2" 2;
verify "SELECT totalFileSize FROM storage WHERE id=$storageId2" 1000;
verify "SELECT totalFileCountNewest FROM storage WHERE id=$storageId2" 2;
verify "SELECT totalFileSizeNewest FROM storage WHERE id=$storageId2" 1000;

verify "SELECT totalEntryCount FROM entities WHERE id=$entityId2" 2;
verify "SELECT totalFileCount FROM entities WHERE id=$entityId2" 2;
verify "SELECT totalFileSize FROM entities WHERE id=$entityId2" 1000;

echo "--- after delete -----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
$SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
$SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM storage"
#echo "Entries:"
#$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type,timeLastChanged,offset,size FROM entries"
#echo "Newest entries:"
#$SQLITE3 $databaseFile -H "SELECT id,entryId,storageId,entryId,name,type,offset,size FROM entriesNewest"
echo "----------------------------------------------------------------------"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
fi

if false; then
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# assign
id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'af1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id1,1000,0,500)"
id2=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'af1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (storageId,entryId,size,fragmentOffset,fragmentSize) VALUES ($storageId2,$id2,1000,0,500)"

id3=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_IMAGE,'ai1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId2,$id3,1000,1,0,500)"
id4=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_IMAGE,'ai1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (storageId,entryId,size,blockSize,blockOffset,blockCount) VALUES ($storageId2,$id4,1000,1,0,500)"

echo "--- before assign ----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
$SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
$SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM storage"
$SQLITE3 $databaseFile -H "SELECT id,totalFileCountNewest,totalFileSizeNewest,totalImageCountNewest,totalImageSizeNewest,totalDirectoryCountNewest,totalLinkCountNewest,totalHardlinkCountNewest,totalHardlinkSizeNewest,totalSpecialCountNewest FROM storage"
#echo "Entries:"
#$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type,timeLastChanged,offset,size FROM entries"
echo "----------------------------------------------------------------------"

$SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId3 WHERE id=$id4"
$SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId3 WHERE id=$id3"
$SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId3 WHERE id=$id2"
$SQLITE3 $databaseFile "UPDATE entries SET storageId=$storageId3 WHERE id=$id1"

echo "--- after assign -----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
$SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
$SQLITE3 $databaseFile -H "SELECT id,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM storage"
$SQLITE3 $databaseFile -H "SELECT id,totalFileCountNewest,totalFileSizeNewest,totalImageCountNewest,totalImageSizeNewest,totalDirectoryCountNewest,totalLinkCountNewest,totalHardlinkCountNewest,totalHardlinkSizeNewest,totalSpecialCountNewest FROM storage"
#echo "Entries:"
#$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type,timeLastChanged,offset,size FROM entries"
echo "----------------------------------------------------------------------"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
fi

$SQLITE3 $databaseFile "SELECT * FROM log"
