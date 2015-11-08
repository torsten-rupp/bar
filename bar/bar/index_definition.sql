/* BAR index database definitions

 Note: SQLite3 require syntax "CREATE TABLE foo(" in a single line!

*/
VERSION = 5;

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
CREATE INDEX IF NOT EXISTS storagesIndex ON storage (entityId,name);

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
CREATE INDEX IF NOT EXISTS directoriesIndex ON directories (storageId,name);

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
