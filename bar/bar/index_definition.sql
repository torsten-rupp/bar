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
  totalStorageCount   INTEGER DEFAULT 0,
  totalStorageSize    INTEGER DEFAULT 0,
  totalEntryCount     INTEGER DEFAULT 0,
  totalEntrySize      INTEGER DEFAULT 0,

  totalFileCount      INTEGER DEFAULT 0,
  totalFileSize       INTEGER DEFAULT 0,
  totalImageCount     INTEGER DEFAULT 0,
  totalImageSize      INTEGER DEFAULT 0,
  totalDirectoryCount INTEGER DEFAULT 0,
  totalLinkCount      INTEGER DEFAULT 0,
  totalHardlinkCount  INTEGER DEFAULT 0,
  totalHardlinkSize   INTEGER DEFAULT 0,
  totalSpecialCount   INTEGER DEFAULT 0
);
INSERT OR IGNORE INTO entities (id,jobUUID,scheduleUUID,created,type,parentJobUUID,bidFlag) VALUES (0,'','',0,0,0,0);
CREATE INDEX IF NOT EXISTS entitiesIndex ON entities (jobUUID);

// --- storage ---------------------------------------------------------
CREATE TABLE IF NOT EXISTS storage(
  id                  INTEGER PRIMARY KEY,
  entityId            INTEGER,
  name                TEXT NOT NULL,
  created             INTEGER,
  size                INTEGER,
  state               INTEGER,
  mode                INTEGER,
  lastChecked         INTEGER,
  errorMessage        TEXT,

  // updated by triggers
  totalEntryCount     INTEGER DEFAULT 0,
  totalEntrySize      INTEGER DEFAULT 0,

  totalFileCount      INTEGER DEFAULT 0,
  totalFileSize       INTEGER DEFAULT 0,
  totalImageCount     INTEGER DEFAULT 0,
  totalImageSize      INTEGER DEFAULT 0,
  totalDirectoryCount INTEGER DEFAULT 0,
  totalLinkCount      INTEGER DEFAULT 0,
  totalHardlinkCount  INTEGER DEFAULT 0,
  totalHardlinkSize   INTEGER DEFAULT 0,
  totalSpecialCount   INTEGER DEFAULT 0

  // Note: no foreign key entityId: storage may be created temporary before an entity
  // FOREIGN KEY(entityId) REFERENCES entities(id)
);
CREATE INDEX IF NOT EXISTS storagesIndex ON storage (entityId,name);

// full-text-search
CREATE VIRTUAL TABLE FTS_storage USING FTS4(
  storageId,
  name,

//  tokenize=unicode61 'tokenchars= !"#$%&''()*+,-:;<=>?@[\]^_`{|}~' 'separators=/.' 'remove_diacritics=0'
  tokenize=unicode61
);

// update triggeres
CREATE TRIGGER storageInsert AFTER INSERT ON storage
  BEGIN
    UPDATE entities
      SET totalStorageCount=totalStorageCount+1,
          totalStorageSize =totalStorageSize +NEW.totalEntrySize,
          totalEntryCount =totalEntryCount +NEW.totalEntryCount,
          totalEntrySize  =totalEntrySize  +NEW.totalEntrySize,

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
CREATE TRIGGER storageDelete BEFORE DELETE ON storage
  BEGIN
    UPDATE entities
      SET totalStorageCount=totalStorageCount-1,
          totalStorageSize =totalStorageSize -OLD.totalEntrySize,
          totalEntryCount =totalEntryCount -OLD.totalEntryCount,
          totalEntrySize  =totalEntrySize  -OLD.totalEntrySize,

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
CREATE TRIGGER storageUpdateSize AFTER UPDATE OF entityId,totalEntryCount,totalEntrySize ON storage
  BEGIN
    UPDATE entities
      SET totalStorageCount=totalStorageCount-1,
          totalStorageSize =totalStorageSize -OLD.totalEntrySize,
          totalEntryCount =totalEntryCount -OLD.totalEntryCount,
          totalEntrySize  =totalEntrySize  -OLD.totalEntrySize,

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
      SET totalStorageCount=totalStorageCount+1,
          totalStorageSize =totalStorageSize +NEW.totalEntrySize,
          totalEntryCount =totalEntryCount +NEW.totalEntryCount,
          totalEntrySize  =totalEntrySize  +NEW.totalEntrySize,

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
CREATE TRIGGER storageUpdateName AFTER UPDATE OF name ON storage
  BEGIN
    DELETE FROM FTS_storage WHERE storageId MATCH OLD.id;
    INSERT INTO FTS_storage VALUES (NEW.id,NEW.name);
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

  FOREIGN KEY(storageId) REFERENCES storage(id)
);
CREATE INDEX IF NOT EXISTS filesIndex ON files (storageId,name);

// full-text-search
CREATE VIRTUAL TABLE FTS_files USING FTS4(
  fileId,
  name,

  tokenize=unicode61
);

// update triggeres
CREATE TRIGGER filesInsert AFTER INSERT ON files
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalFileCount =totalFileCount+1,
          totalFileSize  =totalFileSize +NEW.size
      WHERE storage.id=NEW.storageId;
    INSERT INTO FTS_files VALUES (NEW.id,NEW.name);
  END;
CREATE TRIGGER filesDelete BEFORE DELETE ON files
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalFileCount =totalFileCount-1,
          totalFileSize  =totalFileSize -OLD.size
      WHERE storage.id=OLD.storageId;
    DELETE FROM FTS_files WHERE fileId MATCH OLD.id;
  END;
CREATE TRIGGER filesUpdateSize AFTER UPDATE OF storageId,size ON files
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalFileCount =totalFileCount-1,
          totalFileSize  =totalFileSize -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalFileCount =totalFileCount+1,
          totalFileSize  =totalFileSize +NEW.size
      WHERE storage.id=NEW.storageId;
  END;
CREATE TRIGGER filesUpdateName AFTER UPDATE OF name ON files
  BEGIN
    DELETE FROM FTS_files WHERE fileId MATCH OLD.id;
    INSERT INTO FTS_files VALUES (NEW.id,NEW.name);
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
CREATE INDEX IF NOT EXISTS imagesIndex ON images (storageId,name);

// full-text-search
CREATE VIRTUAL TABLE FTS_images USING FTS4(
  imageId,
  name,

  tokenize=unicode61
);

// update triggeres
CREATE TRIGGER imagesInsert AFTER INSERT ON images
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalImageCount =totalImageCount+1,
          totalImageSize  =totalImageSize +NEW.size
      WHERE storage.id=NEW.storageId;
    INSERT INTO FTS_images VALUES (NEW.id,NEW.name);
  END;
CREATE TRIGGER imagesDelete BEFORE DELETE ON images
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalImageCount =totalImageCount-1,
          totalImageSize  =totalImageSize -OLD.size
      WHERE storage.id=OLD.storageId;
    DELETE FROM FTS_images WHERE imageId MATCH OLD.id;
  END;
CREATE TRIGGER imagesUpdateSize AFTER UPDATE OF storageId,size ON images
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalImageCount =totalImageCount-1,
          totalImageSize  =totalImageSize -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalImageCount =totalImageCount+1,
          totalImageSize  =totalImageSize +NEW.size
      WHERE storage.id=NEW.storageId;
  END;
CREATE TRIGGER imagesUpdateName AFTER UPDATE OF name ON images
  BEGIN
    DELETE FROM FTS_images WHERE imageId MATCH OLD.id;
    INSERT INTO FTS_images VALUES (NEW.id,NEW.name);
  END;

// --- directories -----------------------------------------------------
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
CREATE INDEX IF NOT EXISTS directoriesIndex ON directories (storageId,name);

// full-text-search
CREATE VIRTUAL TABLE FTS_directories USING FTS4(
  directoryId,
  name,

  tokenize=unicode61
);

// update triggeres
CREATE TRIGGER directoriesInsert AFTER INSERT ON directories
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalDirectoryCount =totalDirectoryCount+1
      WHERE storage.id=NEW.storageId;
    INSERT INTO FTS_directories VALUES (NEW.id,NEW.name);
  END;
CREATE TRIGGER directoriesDelete BEFORE DELETE ON directories
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalDirectoryCount =totalDirectoryCount-1
      WHERE storage.id=OLD.storageId;
    DELETE FROM FTS_directories WHERE directoryId MATCH OLD.id;
  END;
CREATE TRIGGER directoriesUpdateSize AFTER UPDATE ON directories
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalDirectoryCount =totalDirectoryCount-1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalDirectoryCount =totalDirectoryCount+1
      WHERE storage.id=NEW.storageId;
  END;
CREATE TRIGGER directoriesUpdateName AFTER UPDATE OF name ON directories
  BEGIN
    DELETE FROM FTS_directories WHERE directoryId MATCH OLD.id;
    INSERT INTO FTS_directories VALUES (NEW.id,NEW.name);
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
CREATE INDEX IF NOT EXISTS linksIndex ON links (storageId,name);

// full-text-search
CREATE VIRTUAL TABLE FTS_links USING FTS4(
  linkId,
  name,

  tokenize=unicode61
);

// update triggeres
CREATE TRIGGER linksInsert AFTER INSERT ON links
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalLinkCount =totalLinkCount+1
      WHERE storage.id=NEW.storageId;
    INSERT INTO FTS_links VALUES (NEW.id,NEW.name);
  END;
CREATE TRIGGER linksDelete BEFORE DELETE ON links
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalLinkCount =totalLinkCount-1
      WHERE storage.id=OLD.storageId;
    DELETE FROM FTS_links WHERE linkId MATCH OLD.id;
  END;
CREATE TRIGGER linsUpdateSize AFTER UPDATE ON links
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalLinkCount =totalLinkCount-1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalLinkCount =totalLinkCount+1
      WHERE storage.id=NEW.storageId;
  END;
CREATE TRIGGER linksUpdateName AFTER UPDATE OF name ON links
  BEGIN
    DELETE FROM FTS_links WHERE linkId MATCH OLD.id;
    INSERT INTO FTS_links VALUES (NEW.id,NEW.name);
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
CREATE INDEX IF NOT EXISTS hardlinksIndex ON hardlinks (storageId,name);

// full-text-search
CREATE VIRTUAL TABLE FTS_hardlinks USING FTS4(
  hardlinkId,
  name,

  tokenize=unicode61
);

// update triggeres
CREATE TRIGGER hardlinksInsert AFTER INSERT ON hardlinks
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalHardlinkCount =totalHardlinkCount+1,
          totalHardlinkSize  =totalHardlinkSize +NEW.size
      WHERE storage.id=NEW.storageId;
    INSERT INTO FTS_hardlinks VALUES (NEW.id,NEW.name);
  END;
CREATE TRIGGER hardlinksDelete BEFORE DELETE ON hardlinks
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalHardlinkCount =totalHardlinkCount-1,
          totalHardlinkSize  =totalHardlinkSize -OLD.size
      WHERE storage.id=OLD.storageId;
    DELETE FROM FTS_hardlinks WHERE hardlinkId MATCH OLD.id;
  END;
CREATE TRIGGER hardlinksUpdateSize AFTER UPDATE OF storageId,size ON hardlinks
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalEntrySize =totalEntrySize -OLD.size,
          totalHardlinkCount =totalHardlinkCount-1,
          totalHardlinkSize  =totalHardlinkSize -OLD.size
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalEntrySize =totalEntrySize +NEW.size,
          totalHardlinkCount =totalHardlinkCount+1,
          totalHardlinkSize  =totalHardlinkSize +NEW.size
      WHERE storage.id=NEW.storageId;
  END;
CREATE TRIGGER hardlinksUpdateName AFTER UPDATE OF name ON hardlinks
  BEGIN
    DELETE FROM FTS_hardlinks WHERE hardlinkId MATCH OLD.id;
    INSERT INTO FTS_hardlinks VALUES (NEW.id,NEW.name);
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
CREATE INDEX IF NOT EXISTS specialIndex ON special (storageId,name);

// full-text-search
CREATE VIRTUAL TABLE FTS_special USING FTS4(
  specialId,
  name,

  tokenize=unicode61
);

// update triggeres
CREATE TRIGGER specialInsert AFTER INSERT ON special
  BEGIN
    UPDATE storage
      SET totalEntryCount  =totalEntryCount+1,
          totalSpecialCount=totalSpecialCount+1
      WHERE storage.id=NEW.storageId;
    INSERT INTO FTS_special VALUES (NEW.id,NEW.name);
  END;
CREATE TRIGGER specialDelete BEFORE DELETE ON special
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalSpecialCount=totalSpecialCount-1
      WHERE storage.id=OLD.storageId;
    DELETE FROM FTS_special WHERE specialId MATCH OLD.id;
  END;
CREATE TRIGGER specialUpdateSize AFTER UPDATE ON special
  BEGIN
    UPDATE storage
      SET totalEntryCount=totalEntryCount-1,
          totalSpecialCount=totalSpecialCount-1
      WHERE storage.id=OLD.storageId;
    UPDATE storage
      SET totalEntryCount=totalEntryCount+1,
          totalSpecialCount=totalSpecialCount+1
      WHERE storage.id=NEW.storageId;
  END;
CREATE TRIGGER specialUpdateName AFTER UPDATE OF name ON special
  BEGIN
    DELETE FROM FTS_special WHERE specialId MATCH OLD.id;
    INSERT INTO FTS_special VALUES (NEW.id,NEW.name);
  END;

/*

// ---------------------------------------------------------------------

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
