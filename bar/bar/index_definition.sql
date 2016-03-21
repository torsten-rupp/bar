/* BAR index database definitions

 Note: SQLite3 require syntax "CREATE TABLE foo(" in a single line!

*/
VERSION = 6;

PRAGMA foreign_keys = ON;
PRAGMA auto_vacuum = INCREMENTAL;

// --- meta ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS meta(
  name  TEXT UNIQUE,
  value TEXT
);
INSERT OR IGNORE INTO meta (name,value) VALUES ('version',$version);
INSERT OR IGNORE INTO meta (name,value) VALUES ('datetime',DATETIME('now'));

// --- uuids -----------------------------------------------------------
CREATE TABLE IF NOT EXISTS uuids(
  id                  INTEGER PRIMARY KEY,
  jobUUID             TEXT UNIQUE NOT NULL,

  // updated by triggers
  totalEntityCount    INTEGER DEFAULT 0,  // total number of entities
//  totalEntitySize     INTEGER DEFAULT 0,  // total size of entities [bytes]
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
  scheduleUUID        TEXT NOT NULL,
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

//  FOREIGN KEY(storageId) REFERENCES storage(id)
);
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
//          totalEntitySize    =totalEntitySize    -OLD.totalStorageSize,
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
CREATE TRIGGER AFTER UPDATE OF entityId,totalEntryCount,totalEntrySize ON entities
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
  totalSpecialCountNewest   INTEGER DEFAULT 0   // total number of newest special entries

  // Note: no foreign key entityId: storage may be created temporary before an entity
  // FOREIGN KEY(entityId) REFERENCES entities(id)
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
CREATE TRIGGER AFTER UPDATE OF entityId,totalEntryCount,totalEntrySize ON storage
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
//  totalEntryCount       INTEGER DEFAULT 0,  // total number of entries
//  totalEntrySize        INTEGER DEFAULT 0,  // total size of entries [bytes]

//  totalEntryCountNewest INTEGER DEFAULT 0,  // total number of newest entries
//  totalEntrySizeNewest  INTEGER DEFAULT 0,  // total size of newest entries [bytes]

  FOREIGN KEY(storageId) REFERENCES storage(id)
);
CREATE INDEX ON entries (storageId,name);
CREATE INDEX ON entries (name);

// newest entries
CREATE TABLE IF NOT EXISTS entriesNewest(
  id              INTEGER PRIMARY KEY,
  name            TEXT UNIQUE NOT NULL,
  timeLastChanged INTEGER DEFAULT 0,
  storageId       INTEGER DEFAULT 0,
  entryId         INTEGER DEFAULT 0,

  FOREIGN KEY(entryId) REFERENCES entries(id)
);
CREATE INDEX ON entriesNewest (name,timeLastChanged);

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

    // update newest info
    INSERT OR IGNORE INTO entriesNewest
      (name) VALUES (NEW.name);
    UPDATE entriesNewest
      SET timeLastChanged=NEW.timeLastChanged,
          storageId=NEW.storageId,
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

    // update FTS
    DELETE FROM FTS_entries WHERE entryId MATCH OLD.id;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON entries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1
      WHERE storage.id=NEW.storageId;
  END;

//CREATE TRIGGER AFTER UPDATE OF totalEntryCount,totalEntrySize ON entries
//  BEGIN
//    // update count/size in parent entries
//    UPDATE entries
//      SET totalEntryCount=totalEntryCount-OLD.totalEntryCount,
//          totalEntrySize =totalEntrySize -OLD.totalEntrySize
//      WHERE     entries.storageId=OLD.storageId
//            AND entries.name=DIRNAME(OLD.name);
//    UPDATE entries
//      SET totalEntryCount=totalEntryCount+NEW.totalEntryCount,
//          totalEntrySize =totalEntrySize +NEW.totalEntrySize
//      WHERE     entries.storageId=NEW.storageId
//            AND entries.name=DIRNAME(NEW.name);
//  END;

//CREATE TRIGGER AFTER UPDATE OF totalEntryCountNewest,totalEntrySizeNewest ON entries
//  BEGIN
//    // update count/size in parent entries
//    UPDATE entries
//      SET totalEntryCountNewest=totalEntryCountNewest-OLD.totalEntryCountNewest,
//          totalEntrySizeNewest =totalEntrySizeNewest -OLD.totalEntrySizeNewest
//      WHERE     entries.storageId=OLD.storageId
//            AND entries.name=DIRNAME(OLD.name);
//    UPDATE entries
//      SET totalEntryCountNewest=totalEntryCountNewest+NEW.totalEntryCountNewest,
//          totalEntrySizeNewest =totalEntrySizeNewest +NEW.totalEntrySizeNewest
//      WHERE     entries.storageId=NEW.storageId
//            AND entries.name=DIRNAME(NEW.name);
//  END;

CREATE TRIGGER AFTER UPDATE OF name ON entries
  BEGIN
    // update FTS
    DELETE FROM FTS_entries WHERE entryId MATCH OLD.id;
    INSERT INTO FTS_entries VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE ON entriesNewest
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1
      WHERE storage.id=NEW.storageId;

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

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON fileEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntrySize=totalEntrySize+NEW.fragmentSize,
          totalFileCount=totalFileCount+1,
          totalFileSize =totalFileSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.fragmentSize
      WHERE     directoryEntries.storageId=(SELECT storageId FROM entries WHERE id=NEW.entryId)
            AND directoryEntries.name=DIRNAME((SELECT name FROM entries WHERE id=NEW.entryId));
  END;

CREATE TRIGGER BEFORE DELETE ON fileEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntrySize=totalEntrySize-OLD.fragmentSize,
          totalFileCount=totalFileCount-1,
          totalFileSize =totalFileSize +OLD.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON fileEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntrySize=totalEntrySize-OLD.fragmentSize,
          totalFileCount=totalFileCount-1,
          totalFileSize =totalFileSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
    UPDATE storage
      SET totalEntrySize=totalEntrySize+NEW.fragmentSize,
          totalFileCount=totalFileCount+1,
          totalFileSize =totalFileSize +OLD.fragmentSize
      WHERE storage.id=NEW.storageId;
  END;

// --- files -----------------------------------------------------------
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

  // updated by triggers

  FOREIGN KEY(storageId) REFERENCES storage(id)
);
CREATE INDEX ON files (storageId,name);
CREATE INDEX ON files (name,timeLastChanged);

// newest files
CREATE TABLE IF NOT EXISTS filesNewest(
  id              INTEGER PRIMARY KEY,
  name            TEXT UNIQUE NOT NULL,
  size            INTEGER DEFAULT 0,
  timeLastChanged INTEGER DEFAULT 0,
  storageId       INTEGER DEFAULT 0,
  fileId          INTEGER DEFAULT 0,

  FOREIGN KEY(fileId) REFERENCES files(id)
);
CREATE INDEX ON filesNewest (name,timeLastChanged);

// full-text-search
CREATE VIRTUAL TABLE FTS_files USING FTS4(
  fileId,
  name,

  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON files
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalFileCount =totalFileCount +1,
          totalFileSize  =totalFileSize  +NEW.size
      WHERE storage.id=NEW.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size
      WHERE     directoryEntries.storageId=NEW.storageId
            AND directoryEntries.name=DIRNAME(NEW.name);

    // update newest info
    INSERT OR IGNORE INTO filesNewest
      (name) VALUES (NEW.name);
    UPDATE filesNewest
      SET timeLastChanged=NEW.timeLastChanged,
          size=NEW.size,
          storageId=NEW.storageId,
          fileId=NEW.id
      WHERE     name=NEW.name
            AND timeLastChanged<NEW.timeLastChanged;

    // update FTS
    INSERT INTO FTS_files
      VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER BEFORE DELETE ON files
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalFileCount =totalFileCount -1,
          totalFileSize  =totalFileSize  -OLD.size
      WHERE storage.id=OLD.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size
      WHERE     directoryEntries.storageId=OLD.storageId
            AND directoryEntries.name=DIRNAME(OLD.name);

    // update newest info
    DELETE FROM filesNewest
      WHERE name=OLD.name;

    // update FTS
    DELETE FROM FTS_files WHERE fileId MATCH OLD.id;
  END;

CREATE TRIGGER AFTER DELETE ON files
  BEGIN
    // update newest info
    INSERT INTO filesNewest (name,timeLastChanged,size,fileId)
      SELECT name,MAX(timeLastChanged),size,id FROM files WHERE name=OLD.name;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,size ON files
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalFileCount =totalFileCount -1,
          totalFileSize  =totalFileSize  -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalFileCount =totalFileCount +1,
          totalFileSize  =totalFileSize  +NEW.size
      WHERE storage.id=NEW.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size
      WHERE     directoryEntries.storageId=OLD.storageId
            AND directoryEntries.name=DIRNAME(OLD.name);
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size
      WHERE     directoryEntries.storageId=NEW.storageId
            AND directoryEntries.name=DIRNAME(NEW.name);
  END;
CREATE TRIGGER AFTER UPDATE OF name ON files
  BEGIN
    DELETE FROM FTS_files WHERE fileId MATCH OLD.id;
    INSERT INTO FTS_files VALUES (NEW.id,NEW.name);
  END;
CREATE TRIGGER AFTER UPDATE OF timeLastChanged ON files
  BEGIN
    // update newest info
    UPDATE filesNewest
      SET timeLastChanged=NEW.timeLastChanged,
          fileId=NEW.id
      WHERE     name=NEW.name
            AND timeLastChanged<NEW.timeLastChanged;
  END;

CREATE TRIGGER AFTER UPDATE ON filesNewest
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalEntrySizeNewest =totalEntrySizeNewest -OLD.size,
          totalFileCountNewest =totalFileCountNewest -1,
          totalFileSizeNewest  =totalFileSizeNewest  -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalEntrySizeNewest =totalEntrySizeNewest +NEW.size,
          totalFileCountNewest =totalFileCountNewest +1,
          totalFileSizeNewest  =totalFileSizeNewest  +NEW.size
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

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON imageEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.blockSize*NEW.blockCount
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
  END;

CREATE TRIGGER BEFORE DELETE ON imageEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalImageCount=totalImageCount-1,
          totalImageSize =totalImageSize +OLD.blockSize*OLD.blockCount
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON imageEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalImageCount=totalImageCount-1,
          totalImageSize =totalImageSize +OLD.blockSize*OLD.blockCount
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=OLD.entryId);
    UPDATE storage
      SET totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.blockSize*NEW.blockCount
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
  END;

// --- images ----------------------------------------------------------
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
CREATE INDEX ON images (storageId,name);

// newest images
CREATE TABLE IF NOT EXISTS imagesNewest(
  id              INTEGER PRIMARY KEY,
  name            TEXT UNIQUE NOT NULL,
  size            INTEGER DEFAULT 0,
  timeLastChanged INTEGER DEFAULT 0,
  storageId       INTEGER DEFAULT 0,
  imageId         INTEGER DEFAULT 0,

  FOREIGN KEY(imageId) REFERENCES images(id)
);
CREATE INDEX ON imagesNewest (name,timeLastChanged);

// full-text-search
CREATE VIRTUAL TABLE FTS_images USING FTS4(
  imageId,
  name,

  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON images
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.size
      WHERE storage.id=NEW.storageId;

    // update newest info
    INSERT OR IGNORE INTO imagesNewest
      (name) VALUES (NEW.name);
    UPDATE imagesNewest
      SET timeLastChanged=NEW.timeLastChanged,
          size=NEW.size,
          storageId=NEW.storageId,
          imageId=NEW.id
      WHERE     name=NEW.name
            AND timeLastChanged<NEW.timeLastChanged;

    // update FTS
    INSERT INTO FTS_images VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER BEFORE DELETE ON images
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalImageCount=totalImageCount-1,
          totalImageSize =totalImageSize -OLD.size
      WHERE storage.id=OLD.storageId;

    // update FTS
    DELETE FROM FTS_images WHERE imageId MATCH OLD.id;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,size ON images
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalImageCount=totalImageCount-1,
          totalImageSize =totalImageSize -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.size
      WHERE storage.id=NEW.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF name ON images
  BEGIN
    // update FTS
    DELETE FROM FTS_images WHERE imageId MATCH OLD.id;
    INSERT INTO FTS_images VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE ON imagesNewest
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalEntrySizeNewest =totalEntrySizeNewest -OLD.size,
          totalImageCountNewest=totalImageCountNewest-1,
          totalImageSizeNewest =totalImageSizeNewest -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalEntrySizeNewest =totalEntrySizeNewest +NEW.size,
          totalImageCountNewest=totalImageCountNewest+1,
          totalImageSizeNewest =totalImageSizeNewest +NEW.size
      WHERE storage.id=NEW.storageId;
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
CREATE INDEX ON directoryEntries (storageId,name);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON directoryEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalDirectoryCount=totalDirectoryCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);
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

// --- directories -----------------------------------------------------
CREATE TABLE IF NOT EXISTS directories(
  id                    INTEGER PRIMARY KEY,
  storageId             INTEGER,
  name                  TEXT,
  timeLastAccess        INTEGER,
  timeModified          INTEGER,
  timeLastChanged       INTEGER,
  userId                INTEGER,
  groupId               INTEGER,
  permission            INTEGER,

  // updated by triggers
  totalEntryCount       INTEGER DEFAULT 0,  // total number of entries
  totalEntrySize        INTEGER DEFAULT 0,  // total size of entries [bytes]

  totalEntryCountNewest INTEGER DEFAULT 0,  // total number of newest entries
  totalEntrySizeNewest  INTEGER DEFAULT 0,  // total size of newest entries [bytes]

  FOREIGN KEY(storageId) REFERENCES storage(id)
);
CREATE INDEX ON directories (storageId,name);

// newest directories
CREATE TABLE IF NOT EXISTS directoriesNewest(
  id              INTEGER PRIMARY KEY,
  name            TEXT UNIQUE NOT NULL,
  timeLastChanged INTEGER DEFAULT 0,
  storageId       INTEGER DEFAULT 0,
  directoryId     INTEGER DEFAULT 0,

  FOREIGN KEY(directoryId) REFERENCES directories(id)
);
CREATE INDEX ON directoriesNewest (name,timeLastChanged);

// full-text-search
CREATE VIRTUAL TABLE FTS_directories USING FTS4(
  directoryId,
  name,

  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON directories
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount    =totalEntryCount    +1,
          totalDirectoryCount=totalDirectoryCount+1
      WHERE storage.id=NEW.storageId;

    // update newest info
    INSERT OR IGNORE INTO directoriesNewest
      (name) VALUES (NEW.name);
    UPDATE directoriesNewest
      SET timeLastChanged=NEW.timeLastChanged,
          storageId=NEW.storageId,
          directoryId=NEW.id
      WHERE     name=NEW.name
            AND timeLastChanged<NEW.timeLastChanged;

    // update FTS
    INSERT INTO FTS_directories VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER BEFORE DELETE ON directories
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount    =totalEntryCount    -1,
          totalDirectoryCount=totalDirectoryCount-1
      WHERE storage.id=OLD.storageId;

    // update FTS
    DELETE FROM FTS_directories WHERE directoryId MATCH OLD.id;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON directories
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

CREATE TRIGGER AFTER UPDATE OF totalEntryCount,totalEntrySize ON directories
  BEGIN
    // update count/size in parent directories
    UPDATE directories
      SET totalEntryCount=totalEntryCount-OLD.totalEntryCount,
          totalEntrySize =totalEntrySize -OLD.totalEntrySize
      WHERE     directories.storageId=OLD.storageId
            AND directories.name=DIRNAME(OLD.name);
    UPDATE directories
      SET totalEntryCount=totalEntryCount+NEW.totalEntryCount,
          totalEntrySize =totalEntrySize +NEW.totalEntrySize
      WHERE     directories.storageId=NEW.storageId
            AND directories.name=DIRNAME(NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE OF totalEntryCountNewest,totalEntrySizeNewest ON directories
  BEGIN
    // update count/size in parent directories
    UPDATE directories
      SET totalEntryCountNewest=totalEntryCountNewest-OLD.totalEntryCountNewest,
          totalEntrySizeNewest =totalEntrySizeNewest -OLD.totalEntrySizeNewest
      WHERE     directories.storageId=OLD.storageId
            AND directories.name=DIRNAME(OLD.name);
    UPDATE directories
      SET totalEntryCountNewest=totalEntryCountNewest+NEW.totalEntryCountNewest,
          totalEntrySizeNewest =totalEntrySizeNewest +NEW.totalEntrySizeNewest
      WHERE     directories.storageId=NEW.storageId
            AND directories.name=DIRNAME(NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE OF name ON directories
  BEGIN
    // update FTS
    DELETE FROM FTS_directories WHERE directoryId MATCH OLD.id;
    INSERT INTO FTS_directories VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE ON directoriesNewest
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalFileCountNewest =totalFileCountNewest -1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalFileCountNewest =totalFileCountNewest +1
      WHERE storage.id=NEW.storageId;

    // update count/size in parent directories
    UPDATE directories
      SET totalEntryCountNewest=totalEntryCountNewest-1
      WHERE     directories.storageId=OLD.storageId
            AND directories.name=DIRNAME(OLD.name);
    UPDATE directories
      SET totalEntryCountNewest=totalEntryCountNewest+1
      WHERE     directories.storageId=NEW.storageId
            AND directories.name=DIRNAME(NEW.name);
  END;

// --- links -----------------------------------------------------------
CREATE TABLE IF NOT EXISTS linkEntries(
  id              INTEGER PRIMARY KEY,
  entryId         INTEGER,
  destinationName TEXT,

  FOREIGN KEY(entryId) REFERENCES entries(id)
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON linkEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalLinkCount=totalLinkCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

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

// --- links -----------------------------------------------------------
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
CREATE INDEX ON links (storageId,name);

// newest links
CREATE TABLE IF NOT EXISTS linksNewest(
  id              INTEGER PRIMARY KEY,
  name            TEXT UNIQUE NOT NULL,
  timeLastChanged INTEGER DEFAULT 0,
  storageId       INTEGER DEFAULT 0,
  linkId          INTEGER DEFAULT 0,

  FOREIGN KEY(linkId) REFERENCES links(id)
);
CREATE INDEX ON linksNewest (name,timeLastChanged);

// full-text-search
CREATE VIRTUAL TABLE FTS_links USING FTS4(
  linkId,
  name,

  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON links
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalLinkCount =totalLinkCount +1
      WHERE storage.id=NEW.storageId;

    // update newest info
    INSERT OR IGNORE INTO linksNewest
      (name) VALUES (NEW.name);
    UPDATE linksNewest
      SET timeLastChanged=NEW.timeLastChanged,
          storageId=NEW.storageId,
          linkId=NEW.id
      WHERE     name=NEW.name
            AND timeLastChanged<NEW.timeLastChanged;

    // update FTS
    INSERT INTO FTS_links VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER BEFORE DELETE ON links
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalLinkCount =totalLinkCount -1
      WHERE storage.id=OLD.storageId;

    // update FTS
    DELETE FROM FTS_links WHERE linkId MATCH OLD.id;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON links
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalLinkCount =totalLinkCount -1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalLinkCount =totalLinkCount +1
      WHERE storage.id=NEW.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF name ON links
  BEGIN
    // update FTS
    DELETE FROM FTS_links WHERE linkId MATCH OLD.id;
    INSERT INTO FTS_links VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE ON linksNewest
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalFileCountNewest =totalFileCountNewest -1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalFileCountNewest =totalFileCountNewest +1
      WHERE storage.id=NEW.storageId;

    // update count in parent directories
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest-1
      WHERE     directoryEntries.storageId=OLD.storageId
            AND directoryEntries.name=DIRNAME(OLD.name);
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest+1
      WHERE     directoryEntries.storageId=NEW.storageId
            AND directoryEntries.name=DIRNAME(NEW.name);
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

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON hardlinkEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntrySize    =totalEntrySize    +NEW.fragmentSize,
          totalHardlinkCount=totalHardlinkCount+1,
          totalHardlinkSize =totalHardlinkSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

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
          totalHardlinkSize =totalHardlinkSize +OLD.fragmentSize
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

// --- hardlinks -------------------------------------------------------
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
CREATE INDEX ON hardlinks (storageId,name);

// newest hardlinks
CREATE TABLE IF NOT EXISTS hardlinksNewest(
  id              INTEGER PRIMARY KEY,
  name            TEXT UNIQUE NOT NULL,
  size            INTEGER DEFAULT 0,
  timeLastChanged INTEGER DEFAULT 0,
  storageId       INTEGER DEFAULT 0,
  hardlinkId      INTEGER DEFAULT 0,

  FOREIGN KEY(hardlinkId) REFERENCES hardlink(id)
);
CREATE INDEX ON hardlinksNewest (name,timeLastChanged);

// full-text-search
CREATE VIRTUAL TABLE FTS_hardlinks USING FTS4(
  hardlinkId,
  name,

  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON hardlinks
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount   =totalEntryCount   +1,
          totalEntrySize    =totalEntrySize    +NEW.size,
          totalHardlinkCount=totalHardlinkCount+1,
          totalHardlinkSize =totalHardlinkSize +NEW.size
      WHERE storage.id=NEW.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size
      WHERE     directoryEntries.storageId=NEW.storageId
            AND directoryEntries.name=DIRNAME(NEW.name);

    // update newest info
    INSERT OR IGNORE INTO hardlinksNewest
      (name) VALUES (NEW.name);
    UPDATE hardlinksNewest
      SET timeLastChanged=NEW.timeLastChanged,
          size=NEW.size,
          storageId=NEW.storageId,
          hardlinkId=NEW.id
      WHERE     name=NEW.name
            AND timeLastChanged<NEW.timeLastChanged;

    // update FTS
    INSERT INTO FTS_hardlinks VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER BEFORE DELETE ON hardlinks
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount   =totalEntryCount   -1,
          totalEntrySize    =totalEntrySize    -OLD.size,
          totalHardlinkCount=totalHardlinkCount-1,
          totalHardlinkSize =totalHardlinkSize -OLD.size
      WHERE storage.id=OLD.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size
      WHERE     directoryEntries.storageId=OLD.storageId
            AND directoryEntries.name=DIRNAME(OLD.name);

    // update FTS
    DELETE FROM FTS_hardlinks WHERE hardlinkId MATCH OLD.id;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,size ON hardlinks
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCount   =totalEntryCount   -1,
          totalEntrySize    =totalEntrySize    -OLD.size,
          totalHardlinkCount=totalHardlinkCount-1,
          totalHardlinkSize =totalHardlinkSize -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount   =totalEntryCount   +1,
          totalEntrySize    =totalEntrySize    +NEW.size,
          totalHardlinkCount=totalHardlinkCount+1,
          totalHardlinkSize =totalHardlinkSize +NEW.size
      WHERE storage.id=NEW.storageId;

    // update count/size in parent directories
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size
      WHERE     directoryEntries.storageId=OLD.storageId
            AND directoryEntries.name=DIRNAME(OLD.name);
    UPDATE directoryEntries
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size
      WHERE     directoryEntries.storageId=NEW.storageId
            AND directoryEntries.name=DIRNAME(NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE OF name ON hardlinks
  BEGIN
    // update FTS
    DELETE FROM FTS_hardlinks WHERE hardlinkId MATCH OLD.id;
    INSERT INTO FTS_hardlinks VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE ON hardlinksNewest
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalEntrySizeNewest =totalEntrySizeNewest -OLD.size,
          totalFileCountNewest =totalFileCountNewest -1,
          totalFileSizeNewest  =totalFileSizeNewest  -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalEntrySizeNewest =totalEntrySizeNewest +NEW.size,
          totalFileCountNewest =totalFileCountNewest +1,
          totalFileSizeNewest  =totalFileSizeNewest  +NEW.size
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

// --- special ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS specialEntries(
  id              INTEGER PRIMARY KEY,
  entryId         INTEGER,
  specialType     INTEGER,
  major           INTEGER,
  minor           INTEGER,

  FOREIGN KEY(entryId) REFERENCES entries(id)
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON specialEntries
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalSpecialCount=totalSpecialCount+1
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

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

// --- special ---------------------------------------------------------
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
CREATE INDEX ON special (storageId,name);

// newest special
CREATE TABLE IF NOT EXISTS specialNewest(
  id              INTEGER PRIMARY KEY,
  name            TEXT UNIQUE NOT NULL,
  timeLastChanged INTEGER DEFAULT 0,
  storageId       INTEGER DEFAULT 0,
  specialId       INTEGER DEFAULT 0,

  FOREIGN KEY(specialId) REFERENCES special(id)
);
CREATE INDEX ON specialNewest (name,timeLastChanged);

// full-text-search
CREATE VIRTUAL TABLE FTS_special USING FTS4(
  specialId,
  name,

  tokenize=unicode61
);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON special
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount  =totalEntryCount  +1,
          totalSpecialCount=totalSpecialCount+1
      WHERE storage.id=NEW.storageId;

    // update newest info
    INSERT OR IGNORE INTO specialNewest
      (name) VALUES (NEW.name);
    UPDATE specialNewest
      SET timeLastChanged=NEW.timeLastChanged,
          storageId=NEW.storageId,
          specialId=NEW.id
      WHERE     name=NEW.name
            AND timeLastChanged<NEW.timeLastChanged;

    // update FTS
    INSERT INTO FTS_special VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER BEFORE DELETE ON special
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount  =totalEntryCount  -1,
          totalSpecialCount=totalSpecialCount-1
      WHERE storage.id=OLD.storageId;

    // update FTS
    DELETE FROM FTS_special WHERE specialId MATCH OLD.id;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId ON special
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCount  =totalEntryCount  -1,
          totalSpecialCount=totalSpecialCount-1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount  =totalEntryCount  +1,
          totalSpecialCount=totalSpecialCount+1
      WHERE storage.id=NEW.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF name ON special
  BEGIN
    // update FTS
    DELETE FROM FTS_special WHERE specialId MATCH OLD.id;
    INSERT INTO FTS_special VALUES (NEW.id,NEW.name);
  END;

CREATE TRIGGER AFTER UPDATE ON specialNewest
  BEGIN
    // update count in storage
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest-1,
          totalFileCountNewest =totalFileCountNewest -1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest=totalEntryCountNewest+1,
          totalFileCountNewest =totalFileCountNewest +1
      WHERE storage.id=NEW.storageId;

    // update count in parent directories
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest-1
      WHERE     directoryEntries.storageId=OLD.storageId
            AND directoryEntries.name=DIRNAME(OLD.name);
    UPDATE directoryEntries
      SET totalEntryCountNewest=totalEntryCountNewest+1
      WHERE     directoryEntries.storageId=NEW.storageId
            AND directoryEntries.name=DIRNAME(NEW.name);
  END;

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
INSERT OR IGNORE INTO meta (name,value) VALUES ('version',$version);
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
