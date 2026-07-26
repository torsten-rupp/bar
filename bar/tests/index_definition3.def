/* BAR index database definitions

 Note: SQLite3 require syntax "CREATE TABLE foo(" in a single line!

*/

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
