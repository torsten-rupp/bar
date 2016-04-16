/* BAR index database definitions

 Note: SQLite3 require syntax "CREATE TABLE foo(" in a single line!

*/

// index version
const VERSION = 6

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
// default entity
INSERT OR IGNORE INTO entities (id,jobUUID,scheduleUUID,created,type,parentJobUUID,bidFlag) VALUES (0,'','',0,0,0,0);
CREATE INDEX ON entities (jobUUID,created,type,lastCreated);

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
CREATE TRIGGER AFTER UPDATE OF jobUUID,totalEntryCount,totalEntrySize,totalFileCount,totalFileSize,totalImageCount,totalImageSize,totalDirectoryCount,totalLinkCount,totalHardlinkCount,totalHardlinkSize,totalSpecialCount ON entities
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
  entityId                  INTEGER,
  name                      TEXT NOT NULL,
  created                   INTEGER,
  size                      INTEGER,
  state                     INTEGER,
  mode                      INTEGER,
  lastChecked               INTEGER,
  errorMessage              TEXT,

  // updated by triggers
  totalEntryCount           INTEGER DEFAULT 0,  // total number of entries
  totalEntrySize            INTEGER DEFAULT 0,  // total size of entries [bytes]

  totalFileCount            INTEGER DEFAULT 0,  // total number of file entries
  totalFileSize             INTEGER DEFAULT 0,  // total size of file entries [bytes]
  totalImageCount           INTEGER DEFAULT 0,  // total number of image entries
  totalImageSize            INTEGER DEFAULT 0,  // total size of image entries [bytes]
  totalDirectoryCount       INTEGER DEFAULT 0,  // total number of directory entries
  totalLinkCount            INTEGER DEFAULT 0,  // total number of link entries
  totalHardlinkCount        INTEGER DEFAULT 0,  // total number of hardlink entries
  totalHardlinkSize         INTEGER DEFAULT 0,  // total size of hardlink entries [bytes]
  totalSpecialCount         INTEGER DEFAULT 0,  // total number of special entries

  totalEntryCountNewest     INTEGER DEFAULT 0,  // total number of newest entries
  totalEntrySizeNewest      INTEGER DEFAULT 0,  // total size of newest entries [bytes]

  totalFileCountNewest      INTEGER DEFAULT 0,  // total number of newest file entries
  totalFileSizeNewest       INTEGER DEFAULT 0,  // total size of newest file entries [bytes]
  totalImageCountNewest     INTEGER DEFAULT 0,  // total number of newest image entries
  totalImageSizeNewest      INTEGER DEFAULT 0,  // total size of newest image entries [bytes]
  totalDirectoryCountNewest INTEGER DEFAULT 0,  // total number of newest directory entries
  totalLinkCountNewest      INTEGER DEFAULT 0,  // total number of newest link entries
  totalHardlinkCountNewest  INTEGER DEFAULT 0,  // total number of newest hardlink entries
  totalHardlinkSizeNewest   INTEGER DEFAULT 0,  // total size of newest hardlink entries [bytes]
  totalSpecialCountNewest   INTEGER DEFAULT 0,  // total number of newest special entries

  // Note: no foreign key entityId: storage may be created temporary before an entity
  FOREIGN KEY(entityId) REFERENCES entities(id)
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
create table log(event text);

// --- entries ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS entries(
  id                    INTEGER PRIMARY KEY,
  storageId             INTEGER,
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
  size                  INTEGER DEFAULT 0,  // Note: redundancy for faster access

  FOREIGN KEY(storageId) REFERENCES storage(id)
);
CREATE INDEX ON entries (storageId,type,name);
CREATE INDEX ON entries (name);
CREATE INDEX ON entries (type,name);

// newest entries
CREATE TABLE IF NOT EXISTS entriesNewest(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER DEFAULT 0,       // Note: redundancy for faster access
  type            INTEGER DEFAULT 0,       // Note: redundancy for faster access
  name            TEXT UNIQUE NOT NULL,
  timeLastChanged INTEGER DEFAULT 0,       // Note: redundancy for faster access
  userId          INTEGER DEFAULT 0,       // Note: redundancy for faster access
  groupId         INTEGER DEFAULT 0,       // Note: redundancy for faster access
  permission      INTEGER DEFAULT 0,       // Note: redundancy for faster access

  entryId         INTEGER DEFAULT 0,

  offset          INTEGER DEFAULT 0,       // Note: redundancy for faster access
  size            INTEGER DEFAULT 0        // Note: redundancy for faster access
//  FOREIGN KEY(entryId) REFERENCES entries(id)
);
CREATE INDEX ON entriesNewest (name);
CREATE INDEX ON entriesNewest (type,name);
CREATE INDEX ON entriesNewest (entryId);

// full-text-search
CREATE VIRTUAL TABLE FTS_entries USING FTS4(
  entryId,
  name,

  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON entries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1
      WHERE storage.id=NEW.storageId;

    // insert/update newest info
    INSERT OR IGNORE INTO entriesNewest
      (name) VALUES (NEW.name);
    UPDATE entriesNewest
      SET storageId=NEW.storageId,
          type=NEW.type,
          timeLastChanged=NEW.timeLastChanged,
          userId=NEW.userId,
          groupId=NEW.groupId,
          permission=NEW.permission,
          entryId=NEW.id
      WHERE     name=NEW.name
            AND timeLastChanged<NEW.timeLastChanged;

    // update FTS
    INSERT INTO FTS_entries VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER BEFORE DELETE ON entries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1
      WHERE storage.id=OLD.storageId;

    // delete/update newest info
    DELETE FROM entriesNewest WHERE entryId=OLD.id;
    INSERT OR IGNORE INTO entriesNewest
        (storageId,name,type,size,timeLastChanged,entryId)
      SELECT storageId,name,type,size,MAX(timeLastChanged),id FROM entries WHERE id!=OLD.id AND name=OLD.name;

    // update FTS
    DELETE FROM FTS_entries WHERE entryId MATCH OLD.id;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,size ON entries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size
      WHERE storage.id=NEW.storageId;

    // delete/update newest info
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

CREATE TRIGGER AFTER INSERT ON entriesNewest
  BEGIN
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalEntrySizeNewest =totalEntrySizeNewest +NEW.size
      WHERE storage.id=NEW.storageId;
// insert into log values('insert entriesNewest '||NEW.name||' '||NEW.size);
  END;

CREATE TRIGGER AFTER DELETE ON entriesNewest
  BEGIN
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalEntrySizeNewest =totalEntrySizeNewest -OLD.size
      WHERE storage.id=OLD.storageId;
// insert into log values('delete entriesNewest '||OLD.name||' '||OLD.size);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,size ON entriesNewest
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalEntrySizeNewest =totalEntrySizeNewest -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalEntrySizeNewest =totalEntrySizeNewest +NEW.size
      WHERE storage.id=NEW.storageId;
 insert into log values('update entriesNewest '||NEW.name||' '||OLD.size||'->'||NEW.size);

    // update count/size in parent entries
//    UPDATE entries
//      SET totalEntryCountNewest=totalEntryCountNewest-1,
//          totalEntrySizeNewest =totalEntrySizeNewest -OLD.size
//      WHERE     entries.storageId=OLD.storageId
//            AND entries.name=DIRNAME(OLD.name);
//    UPDATE entries
//      SET totalEntryCountNewest=totalEntryCountNewest+1,
//          totalEntrySizeNewest =totalEntrySizeNewest +NEW.size
//      WHERE     entries.storageId=NEW.storageId
//            AND entries.name=DIRNAME(NEW.name);
  END;

//TODO
// --- skipped entries ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS skippedEntries(
  id              INTEGER PRIMARY KEY,
  storageId       INTEGER,
  type            INTEGER,
  name            TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,

  FOREIGN KEY(storageId) REFERENCES storage(id)
);
CREATE INDEX ON skippedEntries (name);
CREATE INDEX ON skippedEntries (type);

// --- files -----------------------------------------------------------
CREATE TABLE IF NOT EXISTS fileEntries(
  id              INTEGER PRIMARY KEY,
  entryId         INTEGER,
  size            INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER,

  // updated by triggers

  FOREIGN KEY(entryId) REFERENCES entries(id)
);
CREATE INDEX ON fileEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON fileEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalFileCount=totalFileCount+1,
          totalFileSize =totalFileSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

    UPDATE storage
      SET totalFileCountNewest=totalFileCountNewest+1,
          totalFileSizeNewest =totalFileSizeNewest +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entriesNewest WHERE entryId=NEW.entryId);

    // update offset/size in entry/newest entry
    UPDATE entries
      SET offset=NEW.fragmentOffset,
          size  =NEW.fragmentSize
      WHERE id=NEW.entryId;
    UPDATE entriesNewest
      SET offset=NEW.fragmentOffset,
          size  =NEW.fragmentSize
      WHERE entryId=NEW.entryId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.fragmentSize
      WHERE     directoryEntries.storageId=(SELECT storageId FROM entries WHERE id=NEW.entryId)
            AND directoryEntries.name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON fileEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntrySize=totalEntrySize-OLD.fragmentSize,
          totalFileCount=totalFileCount-1,
          totalFileSize =totalFileSize -OLD.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
  END;


CREATE TRIGGER AFTER UPDATE OF storageId ON fileEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntrySize=totalEntrySize-OLD.fragmentSize,
          totalFileCount=totalFileCount-1,
          totalFileSize =totalFileSize -OLD.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
    UPDATE storage
      SET totalEntrySize=totalEntrySize+NEW.fragmentSize,
          totalFileCount=totalFileCount+1,
          totalFileSize =totalFileSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
  END;

// --- images ----------------------------------------------------------
CREATE TABLE IF NOT EXISTS imageEntries(
  id              INTEGER PRIMARY KEY,
  entryId         INTEGER,
  size            INTEGER,
  fileSystemType  INTEGER,
  blockSize       INTEGER,
  blockOffset     INTEGER,
  blockCount      INTEGER,

  FOREIGN KEY(entryId) REFERENCES entries(id)
);
CREATE INDEX ON imageEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON imageEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.blockCount*NEW.blockSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

    UPDATE storage
      SET totalImageCountNewest=totalImageCountNewest+1,
          totalImageSizeNewest =totalImageSizeNewest +NEW.blockOffset*NEW.blockSize
      WHERE storage.id=(SELECT storageId FROM entriesNewest WHERE entryId=NEW.entryId);

    // update offset/size in entry/newest entry
    UPDATE entries
      SET offset=NEW.blockOffset*NEW.blockSize,
          size  =NEW.blockCount *NEW.blockSize
      WHERE id=NEW.entryId;
    UPDATE entriesNewest
      SET offset=NEW.blockOffset*NEW.blockSize,
          size  =NEW.blockCount *NEW.blockSize
      WHERE entryId=NEW.entryId;
  END;

CREATE TRIGGER BEFORE DELETE ON imageEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalImageCount=totalImageCount-1,
          totalImageSize =totalImageSize -OLD.blockSize*OLD.blockCount
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON imageEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalImageCount=totalImageCount-1,
          totalImageSize =totalImageSize -OLD.blockSize*OLD.blockCount
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
    UPDATE storage
      SET totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.blockSize*NEW.blockCount
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
  END;

// --- directories -----------------------------------------------------
CREATE TABLE IF NOT EXISTS directoryEntries(
  id                    INTEGER PRIMARY KEY,
  entryId               INTEGER,
  storageId             INTEGER,            // Note: duplicate storage for faster access parent directories
  name                  TEXT,               // Note: duplicate storage for faster access parent directories

  // updated by triggers
  totalEntryCount       INTEGER DEFAULT 0,  // total number of entries
  totalEntrySize        INTEGER DEFAULT 0,  // total size of entries [bytes]

  totalEntryCountNewest INTEGER DEFAULT 0,  // total number of newest entries
  totalEntrySizeNewest  INTEGER DEFAULT 0,  // total size of newest entries [bytes]

  FOREIGN KEY(entryId) REFERENCES entries(id)
);
CREATE INDEX ON directoryEntries (entryId);
CREATE INDEX ON directoryEntries (storageId,name);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON directoryEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalDirectoryCount=totalDirectoryCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

    UPDATE storage
      SET totalDirectoryCountNewest=totalDirectoryCountNewest+1
      WHERE storage.id=(SELECT storageId FROM entriesNewest WHERE entryId=NEW.entryId);
  END;

CREATE TRIGGER BEFORE DELETE ON directoryEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalDirectoryCount=totalDirectoryCount-1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON directoryEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalDirectoryCount=totalDirectoryCount-1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
    UPDATE storage
      SET totalDirectoryCount=totalDirectoryCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
  END;

// --- links -----------------------------------------------------------
CREATE TABLE IF NOT EXISTS linkEntries(
  id              INTEGER PRIMARY KEY,
  entryId         INTEGER,
  destinationName TEXT,

  FOREIGN KEY(entryId) REFERENCES entries(id)
);
CREATE INDEX ON linkEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON linkEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalLinkCount=totalLinkCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

    UPDATE storage
      SET totalLinkCountNewest=totalLinkCountNewest+1
      WHERE storage.id=(SELECT storageId FROM entriesNewest WHERE entryId=NEW.entryId);

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1
      WHERE     directoryEntries.storageId=(SELECT storageId FROM entries WHERE id=NEW.entryId)
            AND directoryEntries.name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON linkEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalLinkCount=totalLinkCount-1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON linkEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalLinkCount=totalLinkCount-1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
    UPDATE storage
      SET totalLinkCount=totalLinkCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
  END;

// --- hardlinks -------------------------------------------------------
CREATE TABLE IF NOT EXISTS hardlinkEntries(
  id              INTEGER PRIMARY KEY,
  entryId         INTEGER,
  size            INTEGER,
  fragmentOffset  INTEGER,
  fragmentSize    INTEGER,

  FOREIGN KEY(entryId) REFERENCES entries(id)
);
CREATE INDEX ON hardlinkEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON hardlinkEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalHardlinkCount=totalHardlinkCount+1,
          totalHardlinkSize =totalHardlinkSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

    UPDATE storage
      SET totalHardlinkCountNewest=totalHardlinkCountNewest+1,
          totalHardlinkSizeNewest =totalHardlinkSizeNewest +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entriesNewest WHERE entryId=NEW.entryId);

    // update offset/size in entry/newest entry
    UPDATE entries
      SET offset=NEW.fragmentOffset,
          size  =NEW.fragmentSize
      WHERE id=NEW.entryId;
    UPDATE entriesNewest
      SET offset=NEW.fragmentOffset,
          size  =NEW.fragmentSize
      WHERE entryId=NEW.entryId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.fragmentSize
      WHERE     directoryEntries.storageId=(SELECT storageId FROM entries WHERE id=NEW.entryId)
            AND directoryEntries.name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON hardlinkEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntrySize    =totalEntrySize    -OLD.fragmentSize,
          totalHardlinkCount=totalHardlinkCount-1,
          totalHardlinkSize =totalHardlinkSize -OLD.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON hardlinkEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntrySize    =totalEntrySize    -OLD.fragmentSize,
          totalHardlinkCount=totalHardlinkCount-1,
          totalHardlinkSize =totalHardlinkSize -OLD.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
    UPDATE storage
      SET totalEntrySize    =totalEntrySize    +NEW.fragmentSize,
          totalHardlinkCount=totalHardlinkCount+1,
          totalHardlinkSize =totalHardlinkSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
  END;

// --- special ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS specialEntries(
  id              INTEGER PRIMARY KEY,
  entryId         INTEGER,
  specialType     INTEGER,
  major           INTEGER,
  minor           INTEGER,

  FOREIGN KEY(entryId) REFERENCES entries(id)
);
CREATE INDEX ON specialEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON specialEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalSpecialCount=totalSpecialCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

    UPDATE storage
      SET totalSpecialCountNewest=totalSpecialCountNewest+1
      WHERE storage.id=(SELECT storageId FROM entriesNewest WHERE entryId=NEW.entryId);

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1
      WHERE     directoryEntries.storageId=(SELECT storageId FROM entries WHERE id=NEW.entryId)
            AND directoryEntries.name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON specialEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalSpecialCount=totalSpecialCount-1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON specialEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalSpecialCount=totalSpecialCount-1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
    UPDATE storage
      SET totalSpecialCount=totalSpecialCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
  END;

// --- history ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS history(
  id              INTEGER PRIMARY KEY,
  jobUUID         TEXT NOT NULL,
  scheduleUUID    TEXT,
  type            INTEGER,
  created         INTEGER,
  errorMessage    TEXT,
  totalEntryCount INTEGER,
  totalEntrySize  INTEGER,
  duration        INTEGER
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
