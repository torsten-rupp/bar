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
CREATE INDEX ON entries (storageId,name);
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
          size=NEW.size,
          timeLastChanged=NEW.timeLastChanged,
          userId=NEW.userId,
          groupId=NEW.groupId,
          permission=NEW.permission,
          entryId=NEW.id,
          offset=NEW.offset,
          size=NEW.size
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
    DELETE FROM entriesNewest WHERE entryId=OLD.entryId;
    INSERT OR IGNORE INTO entriesNewest
        (storageId,name,type,size,timeLastChanged,entryId)
      SELECT storageId,name,type,size,MAX(timeLastChanged),id FROM entries WHERE id!=OLD.entryId AND name=OLD.name;

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

CREATE TRIGGER AFTER INSERT ON entriesNewest
  BEGIN
    UPDATE storage
      SET totalEntryCountNewest     =totalEntryCountNewest    +1,
          totalEntrySizeNewest      =totalEntrySizeNewest     +NEW.size,
          totalFileCountNewest      =totalFileCountNewest     +CASE WHEN NEW.type==5 THEN 1        ELSE 0 END,
          totalFileSizeNewest       =totalFileSizeNewest      +CASE WHEN NEW.type==5 THEN NEW.size ELSE 0 END,
          totalImageCountNewest     =totalImageCountNewest    +CASE WHEN NEW.type==6 THEN 1        ELSE 0 END,
          totalImageSizeNewest      =totalImageSizeNewest     +CASE WHEN NEW.type==6 THEN NEW.size ELSE 0 END,
          totalDirectoryCountNewest =totalDirectoryCountNewest+CASE WHEN NEW.type==7 THEN 1        ELSE 0 END,
          totalLinkCountNewest      =totalLinkCountNewest     +CASE WHEN NEW.type==8 THEN 1        ELSE 0 END,
          totalHardlinkCountNewest  =totalHardlinkCountNewest +CASE WHEN NEW.type==9 THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest   =totalHardlinkSizeNewest  +CASE WHEN NEW.type==9 THEN NEW.size ELSE 0 END,
          totalSpecialCountNewest   =totalSpecialCountNewest  +CASE WHEN NEW.type==9 THEN 1        ELSE 0 END
      WHERE storage.id=NEW.storageId;
  END;

CREATE TRIGGER AFTER DELETE ON entriesNewest
  BEGIN
    UPDATE storage
      SET totalEntryCountNewest     =totalEntryCountNewest    -1,
          totalEntrySizeNewest      =totalEntrySizeNewest     -OLD.size,
          totalFileCountNewest      =totalFileCountNewest     -CASE WHEN OLD.type==5 THEN 1        ELSE 0 END,
          totalFileSizeNewest       =totalFileSizeNewest      -CASE WHEN OLD.type==5 THEN OLD.size ELSE 0 END,
          totalImageCountNewest     =totalImageCountNewest    -CASE WHEN OLD.type==6 THEN 1        ELSE 0 END,
          totalImageSizeNewest      =totalImageSizeNewest     -CASE WHEN OLD.type==6 THEN OLD.size ELSE 0 END,
          totalDirectoryCountNewest =totalDirectoryCountNewest-CASE WHEN OLD.type==7 THEN 1        ELSE 0 END,
          totalLinkCountNewest      =totalLinkCountNewest     -CASE WHEN OLD.type==8 THEN 1        ELSE 0 END,
          totalHardlinkCountNewest  =totalHardlinkCountNewest -CASE WHEN OLD.type==9 THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest   =totalHardlinkSizeNewest  -CASE WHEN OLD.type==9 THEN OLD.size ELSE 0 END,
          totalSpecialCountNewest   =totalSpecialCountNewest  -CASE WHEN OLD.type==9 THEN 1        ELSE 0 END
      WHERE storage.id=OLD.storageId;
  END;

CREATE TRIGGER AFTER UPDATE OF storageId,size ON entriesNewest
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntryCountNewest     =totalEntryCountNewest    -1,
          totalEntrySizeNewest      =totalEntrySizeNewest     -OLD.size,
          totalFileCountNewest      =totalFileCountNewest     -CASE WHEN OLD.type==5 THEN 1        ELSE 0 END,
          totalFileSizeNewest       =totalFileSizeNewest      -CASE WHEN OLD.type==5 THEN OLD.size ELSE 0 END,
          totalImageCountNewest     =totalImageCountNewest    -CASE WHEN OLD.type==6 THEN 1        ELSE 0 END,
          totalImageSizeNewest      =totalImageSizeNewest     -CASE WHEN OLD.type==6 THEN OLD.size ELSE 0 END,
          totalDirectoryCountNewest =totalDirectoryCountNewest-CASE WHEN OLD.type==7 THEN 1        ELSE 0 END,
          totalLinkCountNewest      =totalLinkCountNewest     -CASE WHEN OLD.type==8 THEN 1        ELSE 0 END,
          totalHardlinkCountNewest  =totalHardlinkCountNewest -CASE WHEN OLD.type==9 THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest   =totalHardlinkSizeNewest  -CASE WHEN OLD.type==9 THEN OLD.size ELSE 0 END,
          totalSpecialCountNewest   =totalSpecialCountNewest  -CASE WHEN OLD.type==9 THEN 1        ELSE 0 END
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCountNewest     =totalEntryCountNewest    +1,
          totalEntrySizeNewest      =totalEntrySizeNewest     +NEW.size,
          totalFileCountNewest      =totalFileCountNewest     +CASE WHEN NEW.type==5 THEN 1        ELSE 0 END,
          totalFileSizeNewest       =totalFileSizeNewest      +CASE WHEN NEW.type==5 THEN NEW.size ELSE 0 END,
          totalImageCountNewest     =totalImageCountNewest    +CASE WHEN NEW.type==6 THEN 1        ELSE 0 END,
          totalImageSizeNewest      =totalImageSizeNewest     +CASE WHEN NEW.type==6 THEN NEW.size ELSE 0 END,
          totalDirectoryCountNewest =totalDirectoryCountNewest+CASE WHEN NEW.type==7 THEN 1        ELSE 0 END,
          totalLinkCountNewest      =totalLinkCountNewest     +CASE WHEN NEW.type==8 THEN 1        ELSE 0 END,
          totalHardlinkCountNewest  =totalHardlinkCountNewest +CASE WHEN NEW.type==9 THEN 1        ELSE 0 END,
          totalHardlinkSizeNewest   =totalHardlinkSizeNewest  +CASE WHEN NEW.type==9 THEN NEW.size ELSE 0 END,
          totalSpecialCountNewest   =totalSpecialCountNewest  +CASE WHEN NEW.type==9 THEN 1        ELSE 0 END
      WHERE storage.id=NEW.storageId;
//insert into log values(printf('after start update entriesNewest %%d',(SELECT totalEntryCountNewest FROM storage WHERE id=NEW.storageId)));

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
CREATE INDEX ON fileEntries (entryId);

// insert/delete/update triggeres
CREATE TRIGGER AFTER INSERT ON fileEntries
  BEGIN
    // update count/size in storage
    UPDATE storage
      SET totalEntrySize=totalEntrySize+NEW.fragmentSize,
          totalFileCount=totalFileCount+1,
          totalFileSize =totalFileSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

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
    // update count in storage
    UPDATE storage
      SET totalImageCount=totalImageCount+1,
          totalImageSize =totalImageSize +NEW.blockSize*NEW.blockCount
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

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
      SET totalEntrySize    =totalEntrySize    +NEW.fragmentSize,
          totalHardlinkCount=totalHardlinkCount+1,
          totalHardlinkSize =totalHardlinkSize +NEW.fragmentSize
      WHERE storage.id=(SELECT storageId FROM entries WHERE id=NEW.entryId);

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
