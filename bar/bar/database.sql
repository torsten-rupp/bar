/* BAR index database definitions

 Note: SQLite3 require syntax "CREATE TABLE foo(" in a single line!

*/

CREATE TABLE archives(
  id              INTEGER PRIMARY KEY,
  name            TEXT,
  datetime        INTEGER
);

CREATE TABLE files(
  id              INTEGER PRIMARY KEY,
  archiveId       INTEGER,
  name            TEXT,
  size            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER
);

CREATE TABLE directories(
  id              INTEGER PRIMARY KEY,
  archiveId       INTEGER,
  name            TEXT,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER
);

CREATE TABLE images(
  id              INTEGER PRIMARY KEY,
  archiveId       INTEGER,
  name            TEXT,
  fileSystemType  INTEGER,
  size            INTEGER,
  blockSize       INTEGER
);

CREATE TABLE links(
  id              INTEGER PRIMARY KEY,
  archiveId       INTEGER,
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
  archiveId       INTEGER,
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

