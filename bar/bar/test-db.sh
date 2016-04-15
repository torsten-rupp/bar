#!/bin/sh

SQLITE3=./bar-sqlite3

databaseFile=bar-index.db

DATETIME1=`date -d "2016-01-01 01:01:01" +%s`
DATETIME2=`date -d "2016-02-02 02:02:02" +%s`
DATETIME3=`date -d "2016-03-03 03:03:03" +%s`
DATETIME4=`date -d "2016-04-04 04:04:04" +%s`

# create
$SQLITE3 $databaseFile -c

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# uuid
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('0000'); SELECT last_insert_rowid();"
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('1234')"
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('5678')"

# entity
entityId1=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('0000','',$DATETIME1,1); SELECT last_insert_rowid();"`
entityId2=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('1234','',$DATETIME1,1); SELECT last_insert_rowid();"`
entityId3=`$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('5678','',$DATETIME1,1); SELECT last_insert_rowid();"`

# storage
storageId1=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId1,'s1',$DATETIME2,4400,1,1); SELECT last_insert_rowid();"`
storageId2=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId2,'s2',$DATETIME3,400,1,1); SELECT last_insert_rowid();"`
storageId3=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId3,'s3',$DATETIME4,0,1,1); SELECT last_insert_rowid();"`

# files
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,5,'f1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,5,'f1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,5,'f2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,5,'f2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,5,'f2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,5,'f2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"

# images
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,6,'i1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,6,'i1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,6,'i2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,6,'i2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,6,'i2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,6,'i2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"

# directories
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,7,'d1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES ($id,$storageId1,'dir1')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,7,'d2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES ($id,$storageId1,'dir2')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,7,'d2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES ($id,$storageId1,'dir2')"

# links
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,8,'l1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES ($id,'destination1')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,8,'l2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES ($id,'destination2')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,8,'l2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES ($id,'destination2')"

# hardlink
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,9,'h1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,9,'h1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,9,'h2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,9,'h2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,9,'h2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,9,'h2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"

# special
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,10,'s1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES ($id,1)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,10,'s2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES ($id,2)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,10,'s2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES ($id,2)"

echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize,totalFileCount,totalFileSize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
$SQLITE3 $databaseFile -H "SELECT id,totalEntryCount,totalEntrySize,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
$SQLITE3 $databaseFile -H "SELECT id,totalEntryCount,totalEntrySize,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount FROM storage"
echo "Entries:"
$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type FROM entries"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if true; then
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# insert+delete
id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,5,'x1',$DATETIME1); SELECT last_insert_rowid();"`
id2=`$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id1,1000,0,500); SELECT last_insert_rowid();"`
id3=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,5,'x1',$DATETIME1); SELECT last_insert_rowid();"`
id4=`$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id3,1000,0,500); SELECT last_insert_rowid();"`

echo "--- after insert -----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"

$SQLITE3 $databaseFile "DELETE FROM fileEntries WHERE id=$id4"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id3"
$SQLITE3 $databaseFile "DELETE FROM fileEntries WHERE id=$id2"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id1"

echo "--- after delete -----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# assign
id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,5,'a1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (28,1000,0,500)"
id2=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,5,'a1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (29,1000,0,500)"

echo "--- before assign ----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"

$SQLITE3 $databaseFile "UPDATE entries SET storageId=3 WHERE id=$id2"
$SQLITE3 $databaseFile "UPDATE entries SET storageId=3 WHERE id=$id1"

echo "--- after assign -----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
fi

#$SQLITE3 $databaseFile "SELECT * FROM log"
