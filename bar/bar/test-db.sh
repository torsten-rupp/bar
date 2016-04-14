#!/bin/sh

SQLITE3=./bar-sqlite3

databaseFile=bar-index.db

# create
$SQLITE3 $databaseFile -c

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# uuid
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('0000')"
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('1234')"
$SQLITE3 $databaseFile "INSERT INTO uuids (jobUUID) VALUES ('5678')"

# entity
$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('0000','',1,1)"
$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('1234','',1,1)"
$SQLITE3 $databaseFile "INSERT INTO entities (jobUUID,scheduleUUID,created,type) VALUES ('5678','',1,1)"

# storage
$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES (1,'s1',2,4400,1,1)"
$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES (2,'s2',3,400,1,1)"
$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES (3,'s3',4,0,1,1)"

# files
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,5,'f1',1)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (1,1000,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,5,'f1',1)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (2,1000,0,500)"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,5,'f2',2)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (3,1000,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,5,'f2',2)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (4,1000,0,500)"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,5,'f2',3)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (5,1000,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,5,'f2',3)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (6,1000,0,500)"

# images
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,6,'i1',1)"
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES (7,1000,1,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,6,'i1',1)"
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES (8,1000,1,0,500)"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,6,'i2',2)"
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES (9,1000,1,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,6,'i2',2)"
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES (10,1000,1,0,500)"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,6,'i2',3)"
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES (11,1000,1,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,6,'i2',3)"
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES (12,1000,1,0,500)"

# directories
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,7,'d1',1)"
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES (13,1,'dir1')"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,7,'d2',2)"
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES (14,1,'dir2')"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,7,'d2',3)"
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES (15,1,'dir2')"

# links
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,8,'l1',1)"
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES (16,'destination1')"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,8,'l2',2)"
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES (17,'destination2')"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,8,'l2',3)"
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES (18,'destination2')"

# hardlinks
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,9,'h1',1)"
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (19,1000,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,9,'h1',1)"
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (20,1000,0,500)"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,9,'h2',2)"
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (21,1000,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,9,'h2',2)"
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (22,1000,0,500)"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,9,'h2',3)"
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (23,1000,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,9,'h2',3)"
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (24,1000,0,500)"

# special
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,10,'s1',1)"
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES (25,1)"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,10,'s2',2)"
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES (26,2)"

$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (1,10,'s2',3)"
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES (27,2)"

echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
echo "Entries:"
$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type FROM entries"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# insert+delete
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (2,5,'x1',1)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (28,1000,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (2,5,'x1',1)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (29,1000,0,500)"

echo "--- after insert -----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"

$SQLITE3 $databaseFile "DELETE FROM fileEntries WHERE id=8"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=29"
$SQLITE3 $databaseFile "DELETE FROM fileEntries WHERE id=7"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=28"

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
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (2,5,'a1',1)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (28,1000,0,500)"
$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES (2,5,'a1',1)"
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES (29,1000,0,500)"

echo "--- before assign ----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"

$SQLITE3 $databaseFile "UPDATE entries SET storageId=3 WHERE id=28"
$SQLITE3 $databaseFile "UPDATE entries SET storageId=3 WHERE id=29"

echo "--- after assign -----------------------------------------------------"
echo "UUIDs:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,totalEntityCount,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM uuids"
echo "Entities:"
$SQLITE3 $databaseFile -H "SELECT id,jobUUID,created,type,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize FROM entities"
echo "Storage:"
$SQLITE3 $databaseFile -H "SELECT id,entityId,name,created,size,totalEntryCount,totalEntrySize,totalEntryCountNewest,totalEntrySizeNewest FROM storage"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#$SQLITE3 $databaseFile "SELECT * FROM log"
