/* BAR index database definitions

 Note: SQLite3 require syntax "CREATE TABLE foo(" in a single line!

*/

// index version
const VERSION = 6

// storage states
const STATE_OK               = 1
const STATE_CREATE           = 2
const STATE_UPDATE_REQUESTED = 3
const STATE_UPDATE           = 4
const STATE_ERROR            = 5

// modes
const MODE_MANUAL = 0
const MODE_AUTO   = 1

// index types
const TYPE_UUID      = 1
const TYPE_ENTITY    = 2
const TYPE_STORAGE   = 3
const TYPE_ENTRY     = 4
const TYPE_FILE      = 5
const TYPE_IMAGE     = 6
const TYPE_DIRECTORY = 7
const TYPE_LINK      = 8
const TYPE_HARDLINK  = 9
const TYPE_SPECIAL   = 10
const TYPE_HISTORY   = 11

PRAGMA foreign_keys = ON;
PRAGMA auto_vacuum = INCREMENTAL;

// --- meta ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS meta(
  name  TEXT UNIQUE,
  value TEXT
);
INSERT OR IGNORE INTO meta (name,value) VALUES ('version',$VERSION);
INSERT OR IGNORE INTO meta (name,value) VALUES ('datetime',DATETIME('now'));

// --- uuids -----------------------------------------------------------
CREATE TABLE IF NOT EXISTS uuids(
  id                  INTEGER PRIMARY KEY,
  jobUUID             TEXT UNIQUE NOT NULL,

  // updated by triggers
  totalEntityCount    INTEGER DEFAULT 0,  // total number of entities
  lastCreated         INTEGER DEFAULT 0,  // date/time last created entity
  lastErrorMessage    TEXT DEFAULT '',    // last entity error message

  totalStorageCount   INTEGER DEFAULT 0,  // total number of storages
  totalStorageSize    INTEGER DEFAULT 0,  // total size of storages [bytes]

  totalEntryCount     INTEGER DEFAULT 0,  // total number of entries
  totalEntrySize      INTEGER DEFAULT 0,  // total size of entries [bytes]

  totalFileCount      INTEGER DEFAULT 0,  // total number of file entries
  totalFileSize       INTEGER DEFAULT 0,  // total size of file entries [bytes]
  totalImageCount     INTEGER DEFAULT 0,  // total number of image entries
  totalImageSize      INTEGER DEFAULT 0,  // total size of image entries [bytes]
  totalDirectoryCount INTEGER DEFAULT 0,  // total number of directory entries
  totalLinkCount      INTEGER DEFAULT 0,  // total number of link entries
  totalHardlinkCount  INTEGER DEFAULT 0,  // total number of hardlink entries
  totalHardlinkSize   INTEGER DEFAULT 0,  // total size of hardlink entries [bytes]
  totalSpecialCount   INTEGER DEFAULT 0   // total number of file entries
);
CREATE INDEX ON uuids (jobUUID);

// --- entities --------------------------------------------------------
CREATE TABLE IF NOT EXISTS entities(
  id                  INTEGER PRIMARY KEY,
  jobUUID             TEXT NOT NULL,
  scheduleUUID        TEXT NOT NULL DEFAULT '',
  created             INTEGER,
  type                INTEGER,
  parentJobUUID       INTEGER,
  bidFlag             INTEGER,

  // updated by triggers
  totalStorageCount   INTEGER DEFAULT 0,  // total number of storages
  totalStorageSize    INTEGER DEFAULT 0,  // total size of storages [bytes]
  lastCreated         INTEGER DEFAULT 0,  // date/time last created storage
  lastErrorMessage    TEXT DEFAULT '',    // last storage error message

  totalEntryCount     INTEGER DEFAULT 0,  // total number of entries
  totalEntrySize      INTEGER DEFAULT 0,  // total size of entries (sum of fragments) [bytes]

  totalFileCount      INTEGER DEFAULT 0,  // total number of file entries
  totalFileSize       INTEGER DEFAULT 0,  // total size of file entries (sum of fragments) [bytes]
  totalImageCount     INTEGER DEFAULT 0,  // total number of image entries
  totalImageSize      INTEGER DEFAULT 0,  // total size of image entries (sum of fragments) [bytes]
  totalDirectoryCount INTEGER DEFAULT 0,  // total number of directory entries
  totalLinkCount      INTEGER DEFAULT 0,  // total number of link entries
  totalHardlinkCount  INTEGER DEFAULT 0,  // total number of hardlink entries
  totalHardlinkSize   INTEGER DEFAULT 0,  // total size of hardlink entries (sum of fragments) [bytes]
  totalSpecialCount   INTEGER DEFAULT 0   // total number of file entries
);
CREATE INDEX ON entities (jobUUID,created,type,lastCreated);

// default entity
INSERT OR IGNORE INTO entities (id,jobUUID,scheduleUUID,created,type,parentJobUUID,bidFlag) VALUES (0,'','',0,0,0,0);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON entities
  BEGIN
    INSERT OR IGNORE INTO uuids
      (jobUUID) VALUES (NEW.jobUUID);
    UPDATE uuids
      SET totalEntityCount   =totalEntityCount   +1,
          lastCreated        =(SELECT entities.lastCreated      FROM entities WHERE entities.jobUUID=NEW.jobUUID ORDER BY entities.lastCreated DESC LIMIT 0,1),
          lastErrorMessage   =(SELECT entities.lastErrorMessage FROM entities WHERE entities.jobUUID=NEW.jobUUID ORDER BY entities.lastCreated DESC LIMIT 0,1),

          totalStorageCount  =totalStorageCount  +NEW.totalStorageCount,
          totalStorageSize   =totalStorageSize   +NEW.totalStorageSize,

          totalEntryCount    =totalEntryCount    +NEW.totalEntryCount,
          totalEntrySize     =totalEntrySize     +NEW.totalEntrySize,

          totalFileCount     =totalFileCount     +NEW.totalFileCount,
          totalFileSize      =totalFileSize      +NEW.totalFileSize,
          totalImageCount    =totalImageCount    +NEW.totalImageCount,
          totalImageSize     =totalSpecialCount  +NEW.totalImageSize,
          totalDirectoryCount=totalDirectoryCount+NEW.totalDirectoryCount,
          totalLinkCount     =totalLinkCount     +NEW.totalLinkCount,
          totalHardlinkCount =totalHardlinkCount +NEW.totalHardlinkCount,
          totalHardlinkSize  =totalHardlinkSize  +NEW.totalHardlinkSize,
          totalSpecialCount  =totalSpecialCount  +NEW.totalSpecialCount
      WHERE uuids.jobUUID=NEW.jobUUID;
  END;
CREATE TRIGGER BEFORE DELETE ON entities
  BEGIN
    UPDATE uuids
      SET totalEntityCount   =totalEntityCount   -1,
          lastCreated        =(SELECT entities.lastCreated      FROM entities WHERE entities.jobUUID=OLD.jobUUID ORDER BY entities.lastCreated DESC LIMIT 0,1),
          lastErrorMessage   =(SELECT entities.lastErrorMessage FROM entities WHERE entities.jobUUID=OLD.jobUUID ORDER BY entities.lastCreated DESC LIMIT 0,1),

          totalStorageCount  =totalStorageCount  -OLD.totalStorageCount,
          totalStorageSize   =totalStorageSize   -OLD.totalStorageSize,

          totalEntryCount    =totalEntryCount    -OLD.totalEntryCount,
          totalEntrySize     =totalEntrySize     -OLD.totalEntrySize,

          totalFileCount     =totalFileCount     -OLD.totalFileCount,
          totalFileSize      =totalFileSize      -OLD.totalFileSize,
          totalImageCount    =totalImageCount    -OLD.totalImageCount,
          totalImageSize     =totalSpecialCount  -OLD.totalImageSize,
          totalDirectoryCount=totalDirectoryCount-OLD.totalDirectoryCount,
          totalLinkCount     =totalLinkCount     -OLD.totalLinkCount,
          totalHardlinkCount =totalHardlinkCount -OLD.totalHardlinkCount,
          totalHardlinkSize  =totalHardlinkSize  -OLD.totalHardlinkSize,
          totalSpecialCount  =totalSpecialCount  -OLD.totalSpecialCount
      WHERE uuids.jobUUID=OLD.jobUUID;
  END;
CREATE TRIGGER AFTER UPDATE OF jobUUID,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount ON entities
  BEGIN
// insert into log values('trigger entities: UPDATE OF jobUUID='||OLD.jobUUID||'->'||NEW.jobUUID||' totalStorageCount='||OLD.totalStorageCount||'->'||NEW.totalStorageCount||' totalStorageSize='||OLD.totalStorageSize||'->'||NEW.totalStorageSize);
    UPDATE uuids
      SET totalEntityCount   =totalEntityCount   -1,
          lastCreated        =(SELECT entities.lastCreated      FROM entities WHERE entities.jobUUID=OLD.jobUUID ORDER BY entities.lastCreated DESC LIMIT 0,1),
          lastErrorMessage   =(SELECT entities.lastErrorMessage FROM entities WHERE entities.jobUUID=OLD.jobUUID ORDER BY entities.lastCreated DESC LIMIT 0,1),

          totalStorageCount  =totalStorageCount  -OLD.totalStorageCount,
          totalStorageSize   =totalStorageSize   -OLD.totalStorageSize,

          totalEntryCount    =totalEntryCount    -OLD.totalEntryCount,
          totalEntrySize     =totalEntrySize     -OLD.totalEntrySize,

          totalFileCount     =totalFileCount     -OLD.totalFileCount,
          totalFileSize      =totalFileSize      -OLD.totalFileSize,
          totalImageCount    =totalImageCount    -OLD.totalImageCount,
          totalImageSize     =totalSpecialCount  -OLD.totalImageSize,
          totalDirectoryCount=totalDirectoryCount-OLD.totalDirectoryCount,
          totalLinkCount     =totalLinkCount     -OLD.totalLinkCount,
          totalHardlinkCount =totalHardlinkCount -OLD.totalHardlinkCount,
          totalHardlinkSize  =totalHardlinkSize  -OLD.totalHardlinkSize,
          totalSpecialCount  =totalSpecialCount  -OLD.totalSpecialCount
      WHERE uuids.jobUUID=OLD.jobUUID;
    UPDATE uuids
      SET totalEntityCount   =totalEntityCount   +1,
          lastCreated        =(SELECT entities.lastCreated      FROM entities WHERE entities.jobUUID=NEW.jobUUID ORDER BY entities.lastCreated DESC LIMIT 0,1),
          lastErrorMessage   =(SELECT entities.lastErrorMessage FROM entities WHERE entities.jobUUID=NEW.jobUUID ORDER BY entities.lastCreated DESC LIMIT 0,1),

          totalStorageCount  =totalStorageCount  +NEW.totalStorageCount,
          totalStorageSize   =totalStorageSize   +NEW.totalStorageSize,

          totalEntryCount    =totalEntryCount    +NEW.totalEntryCount,
          totalEntrySize     =totalEntrySize     +NEW.totalEntrySize,

          totalFileCount     =totalFileCount     +NEW.totalFileCount,
          totalFileSize      =totalFileSize      +NEW.totalFileSize,
          totalImageCount    =totalImageCount    +NEW.totalImageCount,
          totalImageSize     =totalSpecialCount  +NEW.totalImageSize,
          totalDirectoryCount=totalDirectoryCount+NEW.totalDirectoryCount,
          totalLinkCount     =totalLinkCount     +NEW.totalLinkCount,
          totalHardlinkCount =totalHardlinkCount +NEW.totalHardlinkCount,
          totalHardlinkSize  =totalHardlinkSize  +NEW.totalHardlinkSize,
          totalSpecialCount  =totalSpecialCount  +NEW.totalSpecialCount
      WHERE uuids.jobUUID=NEW.jobUUID;
  END;

// --- storage ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS storage(
  id                        INTEGER PRIMARY KEY,
  entityId                  INTEGER NOT NULL REFERENCES entities(id),
  name                      TEXT NOT NULL,
  created                   INTEGER,
  size                      INTEGER DEFAULT 0,
  state                     INTEGER DEFAULT $STATE_CREATE,
  mode                      INTEGER DEFAULT $MODE_MANUAL,
  lastChecked               INTEGER DEFAULT 0,
  errorMessage              TEXT,

  // updated by triggers
  totalEntryCount           INTEGER DEFAULT 0,  // total number of entries
  totalEntrySize            INTEGER DEFAULT 0,  // total size of entries [bytes]

  totalFileCount            INTEGER DEFAULT 0,  // total number of file entries
  totalFileSize             INTEGER DEFAULT 0,  // total size of file entries (sum of fragments) [bytes]
  totalImageCount           INTEGER DEFAULT 0,  // total number of image entries
  totalImageSize            INTEGER DEFAULT 0,  // total size of image entries (sum of fragments) [bytes]
  totalDirectoryCount       INTEGER DEFAULT 0,  // total number of directory entries
  totalLinkCount            INTEGER DEFAULT 0,  // total number of link entries
  totalHardlinkCount        INTEGER DEFAULT 0,  // total number of hardlink entries
  totalHardlinkSize         INTEGER DEFAULT 0,  // total size of hardlink entries (sum of fragments) [bytes]
  totalSpecialCount         INTEGER DEFAULT 0,  // total number of special entries

  totalEntryCountNewest     INTEGER DEFAULT 0,  // total number of newest entries
  totalEntrySizeNewest      INTEGER DEFAULT 0,  // total size of newest entries [bytes]

  totalFileCountNewest      INTEGER DEFAULT 0,  // total number of newest file entries
  totalFileSizeNewest       INTEGER DEFAULT 0,  // total size of newest file entries (sum of fragments) [bytes]
  totalImageCountNewest     INTEGER DEFAULT 0,  // total number of newest image entries
  totalImageSizeNewest      INTEGER DEFAULT 0,  // total size of newest image entries (sum of fragments) [bytes]
  totalDirectoryCountNewest INTEGER DEFAULT 0,  // total number of newest directory entries
  totalLinkCountNewest      INTEGER DEFAULT 0,  // total number of newest link entries
  totalHardlinkCountNewest  INTEGER DEFAULT 0,  // total number of newest hardlink entries
  totalHardlinkSizeNewest   INTEGER DEFAULT 0,  // total size of newest hardlink entries (sum of fragments) [bytes]
  totalSpecialCountNewest   INTEGER DEFAULT 0   // total number of newest special entries
);
CREATE INDEX ON storage (entityId,name,created,state);

// full-text-search
CREATE VIRTUAL TABLE FTS_storage USING FTS4(
  storageId,
  name,

//  tokenize=unicode61 'tokenchars= !"#$%&''()*+,-:;<=>?@[\]^_`{|}~' 'separators=/.' 'remove_diacritics=0'
  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON storage
  BEGIN
// insert into log values('trigger storage: INSERT size='||NEW.size);
    UPDATE entities
      SET totalStorageCount  =totalStorageCount  +1,
          totalStorageSize   =totalStorageSize   +NEW.size,
          lastCreated        =(SELECT storage.created      FROM storage WHERE storage.entityId=NEW.entityId ORDER BY storage.created DESC LIMIT 0,1),
          lastErrorMessage   =(SELECT storage.errorMessage FROM storage WHERE storage.entityId=NEW.entityId ORDER BY storage.created DESC LIMIT 0,1),

          totalEntryCount    =totalEntryCount    +NEW.totalEntryCount,
          totalEntrySize     =totalEntrySize     +NEW.totalEntrySize,

          totalFileCount     =totalFileCount     +NEW.totalFileCount,
          totalFileSize      =totalFileSize      +NEW.totalFileSize,
          totalImageCount    =totalImageCount    +NEW.totalImageCount,
          totalImageSize     =totalSpecialCount  +NEW.totalImageSize,
          totalDirectoryCount=totalDirectoryCount+NEW.totalDirectoryCount,
          totalLinkCount     =totalLinkCount     +NEW.totalLinkCount,
          totalHardlinkCount =totalHardlinkCount +NEW.totalHardlinkCount,
          totalHardlinkSize  =totalHardlinkSize  +NEW.totalHardlinkSize,
          totalSpecialCount  =totalSpecialCount  +NEW.totalSpecialCount
      WHERE entities.id=NEW.entityId;
    INSERT INTO FTS_storage VALUES (NEW.id,NEW.name);
  END;
CREATE TRIGGER BEFORE DELETE ON storage
  BEGIN
    UPDATE entities
      SET totalStorageCount  =totalStorageCount  -1,
          totalStorageSize   =totalStorageSize   -OLD.size,
          lastCreated        =(SELECT storage.created      FROM storage WHERE storage.entityId=OLD.entityId ORDER BY storage.created DESC LIMIT 0,1),
          lastErrorMessage   =(SELECT storage.errorMessage FROM storage WHERE storage.entityId=OLD.entityId ORDER BY storage.created DESC LIMIT 0,1),

          totalEntryCount    =totalEntryCount    -OLD.totalEntryCount,
          totalEntrySize     =totalEntrySize     -OLD.totalEntrySize,

          totalFileCount     =totalFileCount     -OLD.totalFileCount,
          totalFileSize      =totalFileSize      -OLD.totalFileSize,
          totalImageCount    =totalImageCount    -OLD.totalImageCount,
          totalImageSize     =totalSpecialCount  -OLD.totalImageSize,
          totalDirectoryCount=totalDirectoryCount-OLD.totalDirectoryCount,
          totalLinkCount     =totalLinkCount     -OLD.totalLinkCount,
          totalHardlinkCount =totalHardlinkCount -OLD.totalHardlinkCount,
          totalHardlinkSize  =totalHardlinkSize  -OLD.totalHardlinkSize,
          totalSpecialCount  =totalSpecialCount  -OLD.totalSpecialCount
      WHERE entities.id=OLD.entityId;
    DELETE FROM FTS_storage WHERE storageId MATCH OLD.id;
  END;
CREATE TRIGGER AFTER UPDATE OF entityId,size,totalEntryCount,totalEntrySize,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount ON storage
  BEGIN
// insert into log values('trigger storage: UPDATE OF storageId='||OLD.id||' size='||OLD.size||'->'||NEW.size||' totalEntryCount='||OLD.totalEntryCount||'->'||NEW.totalEntryCount);
    UPDATE entities
      SET totalStorageCount  =totalStorageCount  -1,
          totalStorageSize   =totalStorageSize   -OLD.size,
          lastCreated        =(SELECT storage.created      FROM storage WHERE storage.entityId=OLD.entityId ORDER BY storage.created DESC LIMIT 0,1),
          lastErrorMessage   =(SELECT storage.errorMessage FROM storage WHERE storage.entityId=OLD.entityId ORDER BY storage.created DESC LIMIT 0,1),

          totalEntryCount    =totalEntryCount    -OLD.totalEntryCount,
          totalEntrySize     =totalEntrySize     -OLD.totalEntrySize,

          totalFileCount     =totalFileCount     -OLD.totalFileCount,
          totalFileSize      =totalFileSize      -OLD.totalFileSize,
          totalImageCount    =totalImageCount    -OLD.totalImageCount,
          totalImageSize     =totalSpecialCount  -OLD.totalImageSize,
          totalDirectoryCount=totalDirectoryCount-OLD.totalDirectoryCount,
          totalLinkCount     =totalLinkCount     -OLD.totalLinkCount,
          totalHardlinkCount =totalHardlinkCount -OLD.totalHardlinkCount,
          totalHardlinkSize  =totalHardlinkSize  -OLD.totalHardlinkSize,
          totalSpecialCount  =totalSpecialCount  -OLD.totalSpecialCount
      WHERE entities.id=OLD.entityId;
    UPDATE entities
      SET totalStorageCount  =totalStorageCount  +1,
          totalStorageSize   =totalStorageSize   +NEW.size,
          lastCreated        =(SELECT storage.created      FROM storage WHERE storage.entityId=NEW.entityId ORDER BY storage.created DESC LIMIT 0,1),
          lastErrorMessage   =(SELECT storage.errorMessage FROM storage WHERE storage.entityId=NEW.entityId ORDER BY storage.created DESC LIMIT 0,1),

          totalEntryCount    =totalEntryCount    +NEW.totalEntryCount,
          totalEntrySize     =totalEntrySize     +NEW.totalEntrySize,

          totalFileCount     =totalFileCount     +NEW.totalFileCount,
          totalFileSize      =totalFileSize      +NEW.totalFileSize,
          totalImageCount    =totalImageCount    +NEW.totalImageCount,
          totalImageSize     =totalSpecialCount  +NEW.totalImageSize,
          totalDirectoryCount=totalDirectoryCount+NEW.totalDirectoryCount,
          totalLinkCount     =totalLinkCount     +NEW.totalLinkCount,
          totalHardlinkCount =totalHardlinkCount +NEW.totalHardlinkCount,
          totalHardlinkSize  =totalHardlinkSize  +NEW.totalHardlinkSize,
          totalSpecialCount  =totalSpecialCount  +NEW.totalSpecialCount
      WHERE entities.id=NEW.entityId;
  END;
CREATE TRIGGER AFTER UPDATE OF name ON storage
  BEGIN
    DELETE FROM FTS_storage WHERE storageId MATCH OLD.id;
    INSERT INTO FTS_storage VALUES (NEW.id,NEW.name);
  END;

//TODO: remove
//create table log(event TEXT);

// --- entries ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS entries(
  id                    INTEGER PRIMARY KEY,
  storageId             INTEGER NOT NULL REFERENCES storage(id),
  type                  INTEGER,
  name                  TEXT,
  timeLastAccess        INTEGER,
  timeModified          INTEGER,
  timeLastChanged       INTEGER,
  userId                INTEGER,
  groupId               INTEGER,
  permission            INTEGER,

  // updated by triggers
  offset                INTEGER DEFAULT 0,  // Note: redundancy for faster access
  size                  INTEGER DEFAULT 0   // Note: redundancy for faster access
);
CREATE INDEX ON entries (storageId,type,name);
CREATE INDEX ON entries (name);
CREATE INDEX ON entries (type,name);

// newest entries (updated by triggers)
CREATE TABLE IF NOT EXISTS entriesNewest(
  id              INTEGER PRIMARY KEY,
  entryId         INTEGER REFERENCES entries(id),  // no 'NOT NULL'

  storageId       INTEGER DEFAULT 0,       // Note: redundancy for faster access
  type            INTEGER DEFAULT 0,       // Note: redundancy for faster access
  name            TEXT NOT NULL,
  timeLastChanged INTEGER DEFAULT 0,       // Note: redundancy for faster access
  userId          INTEGER DEFAULT 0,       // Note: redundancy for faster access
  groupId         INTEGER DEFAULT 0,       // Note: redundancy for faster access
  permission      INTEGER DEFAULT 0,       // Note: redundancy for faster access
  offset          INTEGER DEFAULT 0,       // Note: redundancy for faster access
  size            INTEGER DEFAULT 0,       // Note: redundancy for faster access

  CONSTRAINT newest UNIQUE (name,offset,size)
);
CREATE INDEX ON entriesNewest (entryId,name,offset,size,timeLastChanged);
CREATE INDEX ON entriesNewest (name,offset,size,timeLastChanged);
CREATE INDEX ON entriesNewest (type,name);
CREATE INDEX ON entriesNewest (entryId);

// full-text-search
CREATE VIRTUAL TABLE FTS_entries USING FTS4(
  entryId,
  name,

//  tokenize=unicode61 'tokenchars= !"#$%&''()*+,-:;<=>?@[\]^_`{|}~' 'separators=/.' 'remove_diacritics=0'
  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON entries
  BEGIN
    // update FTS
    INSERT INTO FTS_entries VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON entries
  BEGIN
    // update storageId in *Entries
    UPDATE fileEntries      SET storageId=NEW.storageId WHERE entryId=NEW.id;
    UPDATE imageEntries     SET storageId=NEW.storageId WHERE entryId=NEW.id;
    UPDATE directoryEntries SET storageId=NEW.storageId WHERE entryId=NEW.id;
    UPDATE linkEntries      SET storageId=NEW.storageId WHERE entryId=NEW.id;
    UPDATE hardlinkEntries  SET storageId=NEW.storageId WHERE entryId=NEW.id;
    UPDATE specialEntries   SET storageId=NEW.storageId WHERE entryId=NEW.id;

    // update newest count/size in storage
    UPDATE storage
      SET totalEntryCountNewest    =totalEntryCountNewest    -1,
          totalEntrySizeNewest     =totalEntrySizeNewest     -OLD.size,
          totalFileCountNewest     =totalFileCountNewest     -CASE OLD.type WHEN $TYPE_FILE      THEN 1        ELSE 0 END,
          totalFileSizeNewest      =totalFileSizeNewest      -CASE OLD.type WHEN $TYPE_FILE      THEN OLD.size ELSE 0 END,
          totalImageCountNewest    =totalImageCountNewest    -CASE OLD.type WHEN $TYPE_IMAGE     THEN 1        ELSE 0 END,
          totalImageSizeNewest     =totalImageSizeNewest     -CASE OLD.type WHEN $TYPE_IMAGE     THEN OLD.size ELSE 0 END,
          totalDirectoryCountNewest=totalDirectoryCountNewest-CASE OLD.type WHEN $TYPE_DIRECTORY THEN 1        ELSE 0 END,
          totalLinkCountNewest     =totalLinkCountNewest     -CASE OLD.type WHEN $TYPE_LINK      THEN 1        ELSE 0 END,
          totalHardlinkCountNewest =totalHardlinkCountNewest -CASE OLD.type WHEN $TYPE_HARDLINK  THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest  =totalHardlinkSizeNewest  -CASE OLD.type WHEN $TYPE_HARDLINK  THEN OLD.size ELSE 0 END,
          totalSpecialCountNewest  =totalSpecialCountNewest  -CASE OLD.type WHEN $TYPE_SPECIAL   THEN 1        ELSE 0 END
      WHERE     EXISTS(SELECT id FROM entriesNewest WHERE entryId=OLD.id)
            AND storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest    =totalEntryCountNewest    +1,
          totalEntrySizeNewest     =totalEntrySizeNewest     +NEW.size,
          totalFileCountNewest     =totalFileCountNewest     +CASE NEW.type WHEN $TYPE_FILE      THEN 1        ELSE 0 END,
          totalFileSizeNewest      =totalFileSizeNewest      +CASE NEW.type WHEN $TYPE_FILE      THEN NEW.size ELSE 0 END,
          totalImageCountNewest    =totalImageCountNewest    +CASE NEW.type WHEN $TYPE_IMAGE     THEN 1        ELSE 0 END,
          totalImageSizeNewest     =totalImageSizeNewest     +CASE NEW.type WHEN $TYPE_IMAGE     THEN NEW.size ELSE 0 END,
          totalDirectoryCountNewest=totalDirectoryCountNewest+CASE NEW.type WHEN $TYPE_DIRECTORY THEN 1        ELSE 0 END,
          totalLinkCountNewest     =totalLinkCountNewest     +CASE NEW.type WHEN $TYPE_LINK      THEN 1        ELSE 0 END,
          totalHardlinkCountNewest =totalHardlinkCountNewest +CASE NEW.type WHEN $TYPE_HARDLINK  THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest  =totalHardlinkSizeNewest  +CASE NEW.type WHEN $TYPE_HARDLINK  THEN NEW.size ELSE 0 END,
          totalSpecialCountNewest  =totalSpecialCountNewest  +CASE NEW.type WHEN $TYPE_SPECIAL   THEN 1        ELSE 0 END
      WHERE     EXISTS(SELECT id FROM entriesNewest WHERE entryId=NEW.id)
            AND storage.id=NEW.storageId;

    // update newest entries
    UPDATE entriesNewest
      SET storageId=NEW.storageId
      WHERE entryId=OLD.id;
  END;

CREATE TRIGGER AFTER UPDATE OF name ON entries
  BEGIN
    // update FTS
    DELETE FROM FTS_entries WHERE entryId MATCH OLD.id;
    INSERT INTO FTS_entries VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE OF offset,size ON entries
  BEGIN
// insert into log values('trigger entries: UPDATE OF offset='||OLD.offset||'->'||NEW.offset||', size='||OLD.size||'->'||NEW.size);
    // insert if not exists
    INSERT OR IGNORE INTO entriesNewest
        (
         entryId,
         storageId,
         type,
         name,
         timeLastChanged,
         userId,
         groupId,
         permission,
         offset,
         size
        )
      VALUES
        (
         NEW.id,
         NEW.storageId,
         NEW.type,
         NEW.name,
         NEW.timeLastChanged,
         NEW.userId,
         NEW.groupId,
         NEW.permission,
         NEW.offset,
         NEW.size
        );

    // update if new entry is newer
    UPDATE entriesNewest
      SET entryId        =NEW.id,
          storageId      =NEW.storageId,
          type           =NEW.type,
          timeLastChanged=NEW.timeLastChanged,
          userId         =NEW.userId,
          groupId        =NEW.groupId,
          permission     =NEW.permission,
          offset         =NEW.offset,
          size           =NEW.size
      WHERE     entryId!=NEW.id
            AND name=NEW.name
            AND offset=NEW.offset
            AND size=NEW.size
            AND NEW.timeLastChanged>timeLastChanged;
  END;

CREATE TRIGGER BEFORE DELETE ON entries
  BEGIN
// insert into log values('trigger entries DELETE: id='||OLD.id||' name='||OLD.name||' size='||OLD.size);
    // delete *Entries
    DELETE FROM fileEntries      WHERE OLD.type=$TYPE_FILE      AND entryId=OLD.id;
    DELETE FROM imageEntries     WHERE OLD.type=$TYPE_IMAGE     AND entryId=OLD.id;
    DELETE FROM directoryEntries WHERE OLD.type=$TYPE_DIRECTORY AND entryId=OLD.id;
    DELETE FROM linkEntries      WHERE OLD.type=$TYPE_LINK      AND entryId=OLD.id;
    DELETE FROM hardlinkEntries  WHERE OLD.type=$TYPE_HARDLINK  AND entryId=OLD.id;
    DELETE FROM specialEntries   WHERE OLD.type=$TYPE_SPECIAL   AND entryId=OLD.id;

    // delete/update newest info
    DELETE FROM entriesNewest WHERE entryId=OLD.id;
    INSERT OR IGNORE INTO entriesNewest
        (entryId,storageId,name,type,size,timeLastChanged,offset,size)
      SELECT id,storageId,name,type,size,MAX(timeLastChanged),offset,size
        FROM entries
        WHERE     id!=OLD.id
              AND name=OLD.name
              AND offset=OLD.offset
              AND size=OLD.size;

    // update FTS
    DELETE FROM FTS_entries WHERE entryId MATCH OLD.id;
  END;

// -------------------------------------------------------

CREATE TRIGGER AFTER INSERT ON entriesNewest
  BEGIN
// insert into log values('trigger entriesNewest INSERT: name='||NEW.name||' size='||NEW.size);
    UPDATE storage
      SET totalEntryCountNewest    =totalEntryCountNewest    +1,
          totalEntrySizeNewest     =totalEntrySizeNewest     +NEW.size,
          totalFileCountNewest     =totalFileCountNewest     +CASE NEW.type WHEN $TYPE_FILE      THEN 1        ELSE 0 END,
          totalFileSizeNewest      =totalFileSizeNewest      +CASE NEW.type WHEN $TYPE_FILE      THEN NEW.size ELSE 0 END,
          totalImageCountNewest    =totalImageCountNewest    +CASE NEW.type WHEN $TYPE_IMAGE     THEN 1        ELSE 0 END,
          totalImageSizeNewest     =totalImageSizeNewest     +CASE NEW.type WHEN $TYPE_IMAGE     THEN NEW.size ELSE 0 END,
          totalDirectoryCountNewest=totalDirectoryCountNewest+CASE NEW.type WHEN $TYPE_DIRECTORY THEN 1        ELSE 0 END,
          totalLinkCountNewest     =totalLinkCountNewest     +CASE NEW.type WHEN $TYPE_LINK      THEN 1        ELSE 0 END,
          totalHardlinkCountNewest =totalHardlinkCountNewest +CASE NEW.type WHEN $TYPE_HARDLINK  THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest  =totalHardlinkSizeNewest  +CASE NEW.type WHEN $TYPE_HARDLINK  THEN NEW.size ELSE 0 END,
          totalSpecialCountNewest  =totalSpecialCountNewest  +CASE NEW.type WHEN $TYPE_SPECIAL   THEN 1        ELSE 0 END
      WHERE storage.id=NEW.storageId;

    // update count/size in parent directories
// insert into log values('  increment '||DIRNAME(NEW.name)||': size='||NEW.size);
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalEntrySizeNewest =totalEntrySizeNewest +NEW.size
      WHERE     directoryEntries.storageId=NEW.storageId
            AND directoryEntries.name=DIRNAME(NEW.name);
// insert into log values('  done');
  END;

CREATE TRIGGER AFTER UPDATE OF entryId ON entriesNewest
  BEGIN
// insert into log values('trigger entriesNewest: UPDATE OF entryId: name='||NEW.name||' entryId='||coalesce(OLD.entryId,'none')||'->'||NEW.entryId||' offset='||OLD.offset||'->'||NEW.offset||' size='||OLD.size||'->'||NEW.size);
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest    =totalEntryCountNewest    -1,
          totalEntrySizeNewest     =totalEntrySizeNewest     -OLD.size,
          totalFileCountNewest     =totalFileCountNewest     -CASE OLD.type WHEN $TYPE_FILE      THEN 1        ELSE 0 END,
          totalFileSizeNewest      =totalFileSizeNewest      -CASE OLD.type WHEN $TYPE_FILE      THEN OLD.size ELSE 0 END,
          totalImageCountNewest    =totalImageCountNewest    -CASE OLD.type WHEN $TYPE_IMAGE     THEN 1        ELSE 0 END,
          totalImageSizeNewest     =totalImageSizeNewest     -CASE OLD.type WHEN $TYPE_IMAGE     THEN OLD.size ELSE 0 END,
          totalDirectoryCountNewest=totalDirectoryCountNewest-CASE OLD.type WHEN $TYPE_DIRECTORY THEN 1        ELSE 0 END,
          totalLinkCountNewest     =totalLinkCountNewest     -CASE OLD.type WHEN $TYPE_LINK      THEN 1        ELSE 0 END,
          totalHardlinkCountNewest =totalHardlinkCountNewest -CASE OLD.type WHEN $TYPE_HARDLINK  THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest  =totalHardlinkSizeNewest  -CASE OLD.type WHEN $TYPE_HARDLINK  THEN OLD.size ELSE 0 END,
          totalSpecialCountNewest  =totalSpecialCountNewest  -CASE OLD.type WHEN $TYPE_SPECIAL   THEN 1        ELSE 0 END
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest    =totalEntryCountNewest    +1,
          totalEntrySizeNewest     =totalEntrySizeNewest     +NEW.SIZE,
          totalFileCountNewest     =totalFileCountNewest     +CASE NEW.type WHEN $TYPE_FILE      THEN 1        ELSE 0 END,
          totalFileSizeNewest      =totalFileSizeNewest      +CASE NEW.type WHEN $TYPE_FILE      THEN NEW.size ELSE 0 END,
          totalImageCountNewest    =totalImageCountNewest    +CASE NEW.type WHEN $TYPE_IMAGE     THEN 1        ELSE 0 END,
          totalImageSizeNewest     =totalImageSizeNewest     +CASE NEW.type WHEN $TYPE_IMAGE     THEN NEW.size ELSE 0 END,
          totalDirectoryCountNewest=totalDirectoryCountNewest+CASE NEW.type WHEN $TYPE_DIRECTORY THEN 1        ELSE 0 END,
          totalLinkCountNewest     =totalLinkCountNewest     +CASE NEW.type WHEN $TYPE_LINK      THEN 1        ELSE 0 END,
          totalHardlinkCountNewest =totalHardlinkCountNewest +CASE NEW.type WHEN $TYPE_HARDLINK  THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest  =totalHardlinkSizeNewest  +CASE NEW.type WHEN $TYPE_HARDLINK  THEN NEW.size ELSE 0 END,
          totalSpecialCountNewest  =totalSpecialCountNewest  +CASE NEW.type WHEN $TYPE_SPECIAL   THEN 1        ELSE 0 END
      WHERE storage.id=NEW.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalEntrySizeNewest =totalEntrySizeNewest -OLD.size
      WHERE     directoryEntries.storageId=OLD.storageId
            AND directoryEntries.name=DIRNAME(OLD.name);
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalEntrySizeNewest =totalEntrySizeNewest +NEW.size
      WHERE     directoryEntries.storageId=NEW.storageId
            AND directoryEntries.name=DIRNAME(NEW.name);
  END;

CREATE TRIGGER BEFORE DELETE ON entriesNewest
  BEGIN
// insert into log values('trigger entriesNewest: DELETE: name='||OLD.name||' entryId='||OLD.entryId||' offset='||OLD.offset||' size='||OLD.size);
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest    =totalEntryCountNewest    -1,
          totalEntrySizeNewest     =totalEntrySizeNewest     -OLD.size,
          totalFileCountNewest     =totalFileCountNewest     -CASE OLD.type WHEN $TYPE_FILE      THEN 1        ELSE 0 END,
          totalFileSizeNewest      =totalFileSizeNewest      -CASE OLD.type WHEN $TYPE_FILE      THEN OLD.size ELSE 0 END,
          totalImageCountNewest    =totalImageCountNewest    -CASE OLD.type WHEN $TYPE_IMAGE     THEN 1        ELSE 0 END,
          totalImageSizeNewest     =totalImageSizeNewest     -CASE OLD.type WHEN $TYPE_IMAGE     THEN OLD.size ELSE 0 END,
          totalDirectoryCountNewest=totalDirectoryCountNewest-CASE OLD.type WHEN $TYPE_DIRECTORY THEN 1        ELSE 0 END,
          totalLinkCountNewest     =totalLinkCountNewest     -CASE OLD.type WHEN $TYPE_LINK      THEN 1        ELSE 0 END,
          totalHardlinkCountNewest =totalHardlinkCountNewest -CASE OLD.type WHEN $TYPE_HARDLINK  THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest  =totalHardlinkSizeNewest  -CASE OLD.type WHEN $TYPE_HARDLINK  THEN OLD.size ELSE 0 END,
          totalSpecialCountNewest  =totalSpecialCountNewest  -CASE OLD.type WHEN $TYPE_SPECIAL   THEN 1        ELSE 0 END
      WHERE storage.id=OLD.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalEntrySizeNewest =totalEntrySizeNewest -OLD.size
      WHERE     directoryEntries.storageId=OLD.storageId
            AND directoryEntries.name=DIRNAME(OLD.name);
  END;

//TODO
// --- skipped entries ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS skippedEntries(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER NOT NULL REFERENCES storage(id),
  type            INTEGER,
  name            TEXT
);
CREATE INDEX ON skippedEntries (storageId,type,name);
CREATE INDEX ON skippedEntries (name);
CREATE INDEX ON skippedEntries (type,name);

// --- files -----------------------------------------------------------
CREATE TABLE IF NOT EXISTS fileEntries(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER NOT NULL REFERENCES storage(id),  // Note: redundancy for faster access
  entryId         INTEGER NOT NULL REFERENCES entries(id),
  size            INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER

  // updated by triggers
);
CREATE INDEX ON fileEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON fileEntries
  BEGIN
    // update offset/size in entry (Note: will trigger insert/update newest!)
    UPDATE entries
      SET offset=NEW.fragmentOffset,
          size  =NEW.fragmentSize
      WHERE id=NEW.entryId;

    // update count/size in storage
// insert into log values('trigger fileEntries: before UPDATE storage totalEntryCount='||(select totalEntryCount from storage where id=NEW.storageId)||' totalEntrySize='||(select totalEntrySize from storage where id=NEW.storageId));
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.fragmentSize,
          totalFileCount =totalFileCount +1,
          totalFileSize  =totalFileSize  +NEW.fragmentSize
      WHERE id=NEW.storageId;
// insert into log values('trigger fileEntries: after UPDATE storage totalEntryCount='||(select totalEntryCount from storage where id=NEW.storageId)||' totalEntrySize='||(select totalEntrySize from storage where id=NEW.storageId));

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.fragmentSize
      WHERE     storageId=NEW.storageId
            AND name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON fileEntries
  BEGIN
// insert into log values('trigger fileEntries: DELETE entryId='||OLD.entryId||' storageId='||OLD.storageId||' fragmentSize='||OLD.fragmentSize);
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.fragmentSize,
          totalFileCount =totalFileCount -1,
          totalFileSize  =totalFileSize  -OLD.fragmentSize
      WHERE id=OLD.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.fragmentSize
      WHERE     storageId=OLD.storageId
            AND name=DIRNAME((SELECT name FROM entries WHERE id=OLD.entryId));
// insert into log values('done');
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,fragmentSize ON fileEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.fragmentSize,
          totalFileCount =totalFileCount -1,
          totalFileSize  =totalFileSize  -OLD.fragmentSize
      WHERE id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.fragmentSize,
          totalFileCount =totalFileCount +1,
          totalFileSize  =totalFileSize  +NEW.fragmentSize
      WHERE id=NEW.storageId;
  END;

// --- images ----------------------------------------------------------
CREATE TABLE IF NOT EXISTS imageEntries(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER NOT NULL REFERENCES storage(id),  // Note: redundancy for faster access
  entryId         INTEGER NOT NULL REFERENCES entries(id),
  size            INTEGER,
  fileSystemType  INTEGER,
  blockSize       INTEGER,                 // size of image block
  blockOffset     INTEGER,                 // block offset [blocks]
  blockCount      INTEGER                  // block count [blocks]
);
CREATE INDEX ON imageEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON imageEntries
  BEGIN
    // update offset/size in entry (Note: will trigger insert/update newest!)
    UPDATE entries
      SET offset=NEW.blockOffset*NEW.blockSize,
          size  =NEW.blockCount *NEW.blockSize
      WHERE id=NEW.entryId;

    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.blockCount*NEW.blockSize,
          totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.blockCount*NEW.blockSize
      WHERE id=NEW.storageId;
  END;

CREATE TRIGGER BEFORE DELETE ON imageEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.blockSize*OLD.blockCount,
          totalImageCount=totalImageCount-1,
          totalImageSize =totalImageSize -OLD.blockSize*OLD.blockCount
      WHERE id=OLD.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,blockSize,blockCount ON imageEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.blockSize*OLD.blockCount,
          totalImageCount=totalImageCount-1,
          totalImageSize =totalImageSize -OLD.blockSize*OLD.blockCount
      WHERE id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.blockSize*OLD.blockCount,
          totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.blockSize*NEW.blockCount
      WHERE id=NEW.storageId;
  END;

// --- directories -----------------------------------------------------
CREATE TABLE IF NOT EXISTS directoryEntries(
  id                    INTEGER PRIMARY KEY,
  storageId             INTEGER NOT NULL REFERENCES storage(id),  // Note: redundancy for faster access
  entryId               INTEGER NOT NULL REFERENCES entries(id),
  name                  TEXT,                                     // Note: redundancy for faster access of parent directories

  // updated by triggers
  totalEntryCount       INTEGER DEFAULT 0,  // total number of entries
  totalEntrySize        INTEGER DEFAULT 0,  // total size of entries [bytes]

  totalEntryCountNewest INTEGER DEFAULT 0,  // total number of newest entries
  totalEntrySizeNewest  INTEGER DEFAULT 0   // total size of newest entries [bytes]
);
CREATE INDEX ON directoryEntries (entryId);
CREATE INDEX ON directoryEntries (storageId,name);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON directoryEntries
  BEGIN
    // update offset/size in entry (Note: will trigger insert/update newest!)
    UPDATE entries
      SET offset=0,
          size  =0
      WHERE id=NEW.entryId;

    // update count in storage
    UPDATE storage
      SET totalEntryCount    =totalEntryCount    +1,
          totalDirectoryCount=totalDirectoryCount+1
      WHERE storage.id=NEW.storageId;

    // update count in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1
      WHERE     storageId=NEW.storageId
            AND name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
// insert into log values('incremtn parent '||DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId)));
  END;

CREATE TRIGGER BEFORE DELETE ON directoryEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount    =totalEntryCount    -1,
          totalDirectoryCount=totalDirectoryCount-1
      WHERE storage.id=OLD.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON directoryEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount    =totalEntryCount    -1,
          totalDirectoryCount=totalDirectoryCount-1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount    =totalEntryCount    +1,
          totalDirectoryCount=totalDirectoryCount+1
      WHERE storage.id=NEW.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF totalEntryCount,totalEntrySize ON directoryEntries
  BEGIN
// insert into log values('trigger directoryEntries: UPDATE OF name='||NEW.name||', totalEntryCount='||OLD.totalEntryCount||'->'||NEW.totalEntryCount||', totalEntrySize='||OLD.totalEntrySize||'->'||NEW.totalEntrySize);
    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+(NEW.totalEntryCount-OLD.totalEntryCount),
          totalEntrySize =totalEntrySize +(NEW.totalEntrySize -OLD.totalEntrySize )
      WHERE     storageId=NEW.storageId
            AND name=DIRNAME(NEW.name);
  END;
CREATE TRIGGER AFTER UPDATE OF totalEntryCountNewest,totalEntrySizeNewest ON directoryEntries
  BEGIN
// insert into log values('trigger directoryEntries: UPDATE OF totalEntryCountNewest='||OLD.totalEntryCountNewest||'->'||NEW.totalEntryCountNewest||', totalEntrySizeNewest='||OLD.totalEntrySizeNewest||'->'||NEW.totalEntrySizeNewest);
    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest+(NEW.totalEntryCountNewest-OLD.totalEntryCountNewest),
          totalEntrySizeNewest =totalEntrySizeNewest +(NEW.totalEntrySizeNewest -OLD.totalEntrySizeNewest )
      WHERE     storageId=NEW.storageId
            AND name=DIRNAME(NEW.name);
// insert into log values('dir up entry id '||NEW.name||' -> '||DIRNAME(NEW.name)||' '||(NEW.totalEntryCountNewest-OLD.totalEntryCountNewest));
  END;

// --- links -----------------------------------------------------------
CREATE TABLE IF NOT EXISTS linkEntries(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER NOT NULL REFERENCES storage(id),  // Note: redundancy for faster access
  entryId         INTEGER NOT NULL REFERENCES entries(id),
  destinationName TEXT
);
CREATE INDEX ON linkEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON linkEntries
  BEGIN
    // update offset/size in entry (Note: will trigger insert/update newest!)
    UPDATE entries
      SET offset=0,
          size  =0
      WHERE id=NEW.entryId;

    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalLinkCount =totalLinkCount +1
      WHERE id=NEW.storageId;

    // update count in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1
      WHERE     storageId=NEW.storageId
            AND name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON linkEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalLinkCount =totalLinkCount -1
      WHERE storage.id=OLD.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON linkEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalLinkCount =totalLinkCount -1
      WHERE id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalLinkCount =totalLinkCount +1
      WHERE id=NEW.storageId;
  END;

// --- hardlinks -------------------------------------------------------
CREATE TABLE IF NOT EXISTS hardlinkEntries(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER NOT NULL REFERENCES storage(id),  // Note: redundancy for faster access
  entryId         INTEGER NOT NULL REFERENCES entries(id),
  size            INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER
);
CREATE INDEX ON hardlinkEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON hardlinkEntries
  BEGIN
    // update offset/size in entry (Note: will trigger insert/update newest!)
    UPDATE entries
      SET offset=NEW.fragmentOffset,
          size  =NEW.fragmentSize
      WHERE id=NEW.entryId;

    // update count/size in storage
    UPDATE storage
      SET totalEntryCount   =totalEntryCount   +1,
          totalEntrySize    =totalEntrySize    +NEW.fragmentSize,
          totalHardlinkCount=totalHardlinkCount+1,
          totalHardlinkSize =totalHardlinkSize +NEW.fragmentSize
      WHERE id=NEW.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.fragmentSize
      WHERE     storageId=NEW.storageId
            AND name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON hardlinkEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount   =totalEntryCount   -1,
          totalEntrySize    =totalEntrySize    -OLD.fragmentSize,
          totalHardlinkCount=totalHardlinkCount-1,
          totalHardlinkSize =totalHardlinkSize -OLD.fragmentSize
      WHERE id=OLD.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,fragmentSize ON hardlinkEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount   =totalEntryCount   -1,
          totalEntrySize    =totalEntrySize    -OLD.fragmentSize,
          totalHardlinkCount=totalHardlinkCount-1,
          totalHardlinkSize =totalHardlinkSize -OLD.fragmentSize
      WHERE id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount   =totalEntryCount   +1,
          totalEntrySize    =totalEntrySize    +NEW.fragmentSize,
          totalHardlinkCount=totalHardlinkCount+1,
          totalHardlinkSize =totalHardlinkSize +NEW.fragmentSize
      WHERE id=NEW.storageId;
  END;

// --- special ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS specialEntries(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER NOT NULL REFERENCES storage(id),  // Note: redundancy for faster access
  entryId         INTEGER NOT NULL REFERENCES entries(id),
  specialType     INTEGER,
  major           INTEGER,
  minor           INTEGER
);
CREATE INDEX ON specialEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON specialEntries
  BEGIN
    // update offset/size in entry (Note: will trigger insert/update newest!)
    UPDATE entries
      SET offset=0,
          size  =0
      WHERE id=NEW.entryId;

    // update count in storage
    UPDATE storage
      SET totalEntryCount  =totalEntryCount  +1,
          totalSpecialCount=totalSpecialCount+1
      WHERE id=NEW.storageId;

    // update count in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1
      WHERE     storageId=NEW.storageId
            AND name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON specialEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount  =totalEntryCount  -1,
          totalSpecialCount=totalSpecialCount-1
      WHERE id=OLD.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON specialEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount  =totalEntryCount  -1,
          totalSpecialCount=totalSpecialCount-1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount  =totalEntryCount  +1,
          totalSpecialCount=totalSpecialCount+1
      WHERE id=NEW.storageId;
  END;

// --- history ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS history(
  id                INTEGER PRIMARY KEY,
  jobUUID           TEXT NOT NULL,
  scheduleUUID      TEXT,
  hostName          TEXT,
  type              INTEGER,
  created           INTEGER,
  errorMessage      TEXT,
  duration          INTEGER,
  totalEntryCount   INTEGER,
  totalEntrySize    INTEGER,
  skippedEntryCount INTEGER,
  skippedEntrySize  INTEGER,
  errorEntryCount   INTEGER,
  errorEntrySize    INTEGER
);
CREATE INDEX ON history (jobUUID,created,type);
CREATE INDEX ON history (created);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/*

// ---------------------------------------------------------------------

VERSION = 5;???

VERSION = 4;

PRAGMA foreign_keys = ON;
PRAGMA auto_vacuum = INCREMENTAL;

CREATE TABLE IF NOT EXISTS meta(
  name  TEXT UNIQUE,
  value TEXT
);
INSERT OR IGNORE INTO meta (name,value) VALUES ('version',$VERSION);
INSERT OR IGNORE INTO meta (name,value) VALUES ('datetime',DATETIME('now'));

CREATE TABLE IF NOT EXISTS entities(
  id              INTEGER PRIMARY KEY,
  jobUUID         TEXT NOT NULL,
  scheduleUUID    TEXT NOT NULL,
  created         INTEGER,
  type            INTEGER,
  parentJobUUID   INTEGER,
  bidFlag         INTEGER
);

CREATE TABLE IF NOT EXISTS storage(
  id              INTEGER PRIMARY KEY,
  entityId        INTEGER,
  name            TEXT NOT NULL,
  created         INTEGER,
  entries         INTEGER,
  size            INTEGER,
  state           INTEGER,
  mode            INTEGER,
  lastChecked     INTEGER,
  errorMessage    TEXT,

  FOREIGN KEY(entityId) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS files(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  size            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER,

  FOREIGN KEY(storageId) REFERENCES storage(id)
);

CREATE TABLE IF NOT EXISTS images(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  fileSystemType  INTEGER,
  size            INTEGER,
  blockSize       INTEGER,
  blockOffset     INTEGER,
  blockCount      INTEGER,

  FOREIGN KEY(storageId) REFERENCES storage(id)
);
CREATE INDEX IF NOT EXISTS imagesIndex ON images (storageId,name);

CREATE TABLE IF NOT EXISTS directories(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,

  FOREIGN KEY(storageId) REFERENCES storage(id)
);

CREATE TABLE IF NOT EXISTS links(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  destinationName TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,

  FOREIGN KEY(storageId) REFERENCES storage(id)
);

CREATE TABLE IF NOT EXISTS hardlinks(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  size            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER,

  FOREIGN KEY(storageId) REFERENCES storage(id)
);

CREATE TABLE IF NOT EXISTS special(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  specialType     INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  major           INTEGER,
  minor           INTEGER,

  FOREIGN KEY(storageId) REFERENCES storage(id)
);


VERSION = 3;

CREATE TABLE meta(
  name  TEXT,
  value TEXT
);
INSERT INTO meta (name,value) VALUES ('version',3);
INSERT INTO meta (name,value) VALUES ('datetime',DATETIME('now'));

CREATE TABLE storage(
  id              INTEGER PRIMARY KEY,
  name            TEXT,
  uuid            TEXT,
  created         INTEGER,
  size            INTEGER,
  state           INTEGER,
  mode            INTEGER,
  lastChecked     INTEGER,
  errorMessage    TEXT
);

CREATE TABLE files(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  size            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER
);

CREATE TABLE images(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  fileSystemType  INTEGER,
  size            INTEGER,
  blockSize       INTEGER,
  blockOffset     INTEGER,
  blockCount      INTEGER
);

CREATE TABLE directories(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER
);

CREATE TABLE links(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  destinationName TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER
);

CREATE TABLE hardlinks(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  size            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER
);

CREATE TABLE special(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  specialType     INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  major           INTEGER,
  minor           INTEGER
);

// ---------------------------------------------------------------------

VERSION = 2;

CREATE TABLE meta(
  name  TEXT,
  value TEXT
);
INSERT INTO meta (name,value) VALUES ('version',2);
INSERT INTO meta (name,value) VALUES ('datetime',DATETIME('now'));

CREATE TABLE storage(
  id              INTEGER PRIMARY KEY,
  name            TEXT,
  created         INTEGER,
  size            INTEGER,
  state           INTEGER,
  mode            INTEGER,
  lastChecked     INTEGER,
  errorMessage    TEXT
);

CREATE TABLE files(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  size            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER
);

CREATE TABLE images(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  fileSystemType  INTEGER,
  size            INTEGER,
  blockSize       INTEGER,
  blockOffset     INTEGER,
  blockCount      INTEGER
);

CREATE TABLE directories(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER
);

CREATE TABLE links(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  destinationName TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER
);

CREATE TABLE hardlinks(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  size            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER
);

CREATE TABLE special(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  specialType     INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  major           INTEGER,
  minor           INTEGER
);


// ---------------------------------------------------------------------

VERSION = 1;

CREATE TABLE meta(
  name  TEXT,
  value TEXT
);
INSERT INTO meta (name,value) VALUES ('version',1);
INSERT INTO meta (name,value) VALUES ('datetime',DATETIME('now'));

CREATE TABLE storage(
  id              INTEGER PRIMARY KEY,
  name            TEXT,
  size            INTEGER,
  created         INTEGER,
  state           INTEGER,
  mode            INTEGER,
  lastChecked     INTEGER,
  errorMessage    TEXT
);

CREATE TABLE files(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  size            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER
);

CREATE TABLE images(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  fileSystemType  INTEGER,
  size            INTEGER,
  blockSize       INTEGER,
  blockOffset     INTEGER,
  blockCount      INTEGER
);

CREATE TABLE directories(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER
);

CREATE TABLE links(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  destinationName TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER
);

CREATE TABLE special(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  name            TEXT,
  specialType     INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
  major           INTEGER,
  minor           INTEGER
);

*/
