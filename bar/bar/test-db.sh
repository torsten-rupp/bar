#!/bin/sh

SQLITE3=./bar-sqlite3

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

# storage
storageId1=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId1,'s1',$DATETIME2,4400,1,1); SELECT last_insert_rowid();"`
storageId2=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId2,'s2',$DATETIME3,400,1,1); SELECT last_insert_rowid();"`
storageId3=`$SQLITE3 $databaseFile "INSERT INTO storage (entityId,name,created,size,state,mode) VALUES ($entityId3,'s3',$DATETIME4,0,1,1); SELECT last_insert_rowid();"`

# files
if false; then
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,500,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,500,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_FILE,'f2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,500,500)"
fi

# images
if false; then
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,500,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,500,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_IMAGE,'i2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id,1000,1,500,500)"
fi

# directories
if false; then
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES ($id,$storageId1,'dir1')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES ($id,$storageId1,'dir2')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_DIRECTORY,'d2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO directoryEntries (entryId,storageId,name) VALUES ($id,$storageId1,'dir2')"
fi

# links
if false; then
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES ($id,'destination1')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES ($id,'destination2')"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_LINK,'l2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO linkEntries (entryId,destinationName) VALUES ($id,'destination2')"
fi

# hardlink
if false; then
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,500,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,500,500)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,0,500)"
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_HARDLINK,'h2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO hardlinkEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id,1000,500,500)"
fi

# special
if true; then
id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES ($id,1)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s2',$DATETIME2); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES ($id,2)"

id=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId1,$TYPE_SPECIAL,'s2',$DATETIME3); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO specialEntries (entryId,specialType) VALUES ($id,2)"
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
$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type,offset,size FROM entries"
echo "Newest entries:"
$SQLITE3 $databaseFile -H "SELECT id,entryId,storageId,entryId,name,type,offset,size FROM entriesNewest"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if false; then
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# insert+delete
id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'x1',$DATETIME1); SELECT last_insert_rowid();"`
id2=`$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id1,1000,0,500); SELECT last_insert_rowid();"`
id3=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'x1',$DATETIME1); SELECT last_insert_rowid();"`
id4=`$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id3,1000,0,500); SELECT last_insert_rowid();"`

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
#echo "Entries:"
#$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type FROM entries"

$SQLITE3 $databaseFile "DELETE FROM fileEntries WHERE id=$id4"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id3"
$SQLITE3 $databaseFile "DELETE FROM fileEntries WHERE id=$id2"
$SQLITE3 $databaseFile "DELETE FROM entries WHERE id=$id1"

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
#$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type FROM entries"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# assign
id1=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'af1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id1,1000,0,500)"
id2=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_FILE,'af1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO fileEntries (entryId,size,fragmentOffset,fragmentSize) VALUES ($id2,1000,0,500)"

id3=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_IMAGE,'ai1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id3,1000,1,0,500)"
id4=`$SQLITE3 $databaseFile "INSERT INTO entries (storageId,type,name,timeLastChanged) VALUES ($storageId2,$TYPE_IMAGE,'ai1',$DATETIME1); SELECT last_insert_rowid();"`
$SQLITE3 $databaseFile "INSERT INTO imageEntries (entryId,size,blockSize,blockOffset,blockCount) VALUES ($id4,1000,1,0,500)"

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
#$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type FROM entries"

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
#$SQLITE3 $databaseFile -H "SELECT id,storageId,name,type FROM entries"
# ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
fi

$SQLITE3 $databaseFile "SELECT * FROM log"
