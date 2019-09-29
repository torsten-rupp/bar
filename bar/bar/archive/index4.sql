PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE meta(  name  TEXT UNIQUE,  value TEXT);
INSERT INTO meta VALUES('version','4');
INSERT INTO meta VALUES('datetime','2019-09-28 07:45:43');
CREATE TABLE entities(  id              INTEGER PRIMARY KEY,  jobUUID         TEXT NOT NULL,  scheduleUUID    TEXT NOT NULL,  created         INTEGER,  type            INTEGER,  parentJobUUID   INTEGER,  bidFlag         INTEGER);
INSERT INTO entities VALUES(1,'8501b352-250f-4a45-877c-569b9e322681','','2019-09-28 07:46:24',1,'',0);
CREATE TABLE storage(  id              INTEGER PRIMARY KEY,  entityId        INTEGER,  name            TEXT NOT NULL,  created         INTEGER,  entries         INTEGER,  size            INTEGER,  state           INTEGER,  mode            INTEGER,  lastChecked     INTEGER,  errorMessage    TEXT,  FOREIGN KEY(entityId) REFERENCES entities(id));
INSERT INTO storage VALUES(1,1,'test.bar','2019-09-28 07:46:24',40,4209682,1,0,'2019-09-28 07:46:24',NULL);
CREATE TABLE files(  id              INTEGER PRIMARY KEY,  storageId       INTEGER,  name            TEXT,  size            INTEGER,  timeLastAccess  INTEGER,  timeModified    INTEGER,  timeLastChanged INTEGER,  userId          INTEGER,  groupId         INTEGER,  permission      INTEGER,  fragmentOffset  INTEGER,  fragmentSize    INTEGER,  FOREIGN KEY(storageId) REFERENCES storage(id));
INSERT INTO files VALUES(1,1,'test/data/zero128.dat',128,1569566935,1569148356,1569148356,1000,1000,33204,0,128);
INSERT INTO files VALUES(2,1,'test/data/random128.dat',128,1569566935,1569148356,1569148356,1000,1000,33204,0,128);
INSERT INTO files VALUES(3,1,'test/data/circular_link.dat.marker',0,1569653032,1569148356,1569148356,1000,1000,33204,0,0);
INSERT INTO files VALUES(4,1,'test/data/file400.dat',1024,1569566935,1569148356,1569148356,1000,1000,33024,0,1024);
INSERT INTO files VALUES(5,1,'test/data/file664.dat',1024,1569566935,1569148356,1569148356,1000,1000,33204,0,1024);
INSERT INTO files VALUES(6,1,'test/data/zero.dat',0,1569566935,1569148356,1569148356,1000,1000,33204,0,0);
INSERT INTO files VALUES(7,1,'test/data/file640.dat',1024,1569566935,1569148356,1569148356,1000,1000,33184,0,1024);
INSERT INTO files VALUES(8,1,'test/data/extended_attribute.dat',1024,1569566935,1569148357,1569148357,1000,1000,33204,0,1024);
INSERT INTO files VALUES(9,1,'test/data/immutable.dat',18,1569566935,1569148357,1569148357,1000,1000,33204,0,18);
INSERT INTO files VALUES(10,1,'test/data/file666.dat',1024,1569566935,1569148356,1569148356,1000,1000,33206,0,1024);
INSERT INTO files VALUES(11,1,'test/data/hardlink_extended_attribute.dat',1024,1569566935,1569148357,1569148357,1000,1000,33204,0,1024);
INSERT INTO files VALUES(12,1,'test/data/hardlinkdata_extended_attribute.dat',1024,1569566935,1569148357,1569148357,1000,1000,33204,0,1024);
INSERT INTO files VALUES(13,1,'test/data/readonly.dat',12,1569566935,1569148356,1569148356,1000,1000,33056,0,12);
INSERT INTO files VALUES(14,1,'test/data/file644.dat',1024,1569566935,1569148356,1569148356,1000,1000,33188,0,1024);
INSERT INTO files VALUES(15,1,'test/data/append-only.dat',17,1569566935,1569148357,1569148357,1000,1000,33204,0,17);
INSERT INTO files VALUES(16,1,'test/data/linkdata.dat',524288,1569566935,1569148356,1569148356,1000,1000,33204,0,524288);
INSERT INTO files VALUES(17,1,'test/data/name\n.dat',12,1569653032,1569588848,1569588848,1000,1000,33204,0,12);
INSERT INTO files VALUES(18,1,'test/data/zero-random512k.dat',524288,1569566935,1569148356,1569148356,1000,1000,33204,0,524288);
INSERT INTO files VALUES(19,1,'test/data/name".dat',12,1569653032,1569588848,1569588848,1000,1000,33204,0,12);
INSERT INTO files VALUES(20,1,'test/data/smallfile.dat',128,1569566935,1569148356,1569148356,1000,1000,33204,0,128);
INSERT INTO files VALUES(21,1,'test/data/name\.dat',12,1569653032,1569588848,1569588848,1000,1000,33204,0,12);
INSERT INTO files VALUES(22,1,'test/data/readonly512k.dat',524288,1569566935,1569148356,1569148356,1000,1000,33056,0,524288);
INSERT INTO files VALUES(23,1,'test/data/name%d%f%s%p.dat',12,1569566935,1569148357,1569148357,1000,1000,33204,0,12);
INSERT INTO files VALUES(24,1,'test/data/zero1024.dat',1024,1569566935,1569148356,1569148356,1000,1000,33204,0,1024);
INSERT INTO files VALUES(25,1,'test/data/hardlink2.dat',524288,1569566935,1569148356,1569148356,1000,1000,33204,0,524288);
INSERT INTO files VALUES(26,1,'test/data/hardlinkdata.dat',524288,1569566935,1569148356,1569148356,1000,1000,33204,0,524288);
INSERT INTO files VALUES(27,1,'test/data/hardlink1.dat',524288,1569566935,1569148356,1569148356,1000,1000,33204,0,524288);
INSERT INTO files VALUES(28,1,'test/data/zero512k.dat',524288,1569566935,1569148356,1569148356,1000,1000,33204,0,524288);
INSERT INTO files VALUES(29,1,'test/data/random1024.dat',1024,1569566935,1569148356,1569148356,1000,1000,33204,0,1024);
INSERT INTO files VALUES(30,1,'test/data/random512k.dat',524288,1569566935,1569148356,1569148356,1000,1000,33204,0,524288);
INSERT INTO files VALUES(31,1,'test/data/delta2/test.dat',524288,1569566935,1569148357,1569148357,1000,1000,33204,0,524288);
INSERT INTO files VALUES(32,1,'test/data/sub_dir/test.dat',12,1569566935,1569148356,1569148356,1000,1000,33204,0,12);
INSERT INTO files VALUES(33,1,'test/data/delta1/test.dat',524288,1569566935,1569148357,1569148357,1000,1000,33204,0,524288);
INSERT INTO files VALUES(34,1,'test/data/name''.dat',12,1569653032,1569588848,1569588848,1000,1000,33204,0,12);
CREATE TABLE images(  id              INTEGER PRIMARY KEY,  storageId       INTEGER,  name            TEXT,  fileSystemType  INTEGER,  size            INTEGER,  blockSize       INTEGER,  blockOffset     INTEGER,  blockCount      INTEGER,  FOREIGN KEY(storageId) REFERENCES storage(id));
INSERT INTO "images" VALUES(1,1,'/dev/loop0',0,33554432,4096,0,8192);
INSERT INTO "images" VALUES(1,2,'/dev/loop1',1,33554432,4096,0,8192);
CREATE TABLE directories(  id              INTEGER PRIMARY KEY,  storageId       INTEGER,  name            TEXT,  timeLastAccess  INTEGER,  timeModified    INTEGER,  timeLastChanged INTEGER,  userId          INTEGER,  groupId         INTEGER,  permission      INTEGER,  FOREIGN KEY(storageId) REFERENCES storage(id));
INSERT INTO directories VALUES(1,1,'test/data',1569566935,1569148357,1569148357,1000,1000,16877);
INSERT INTO directories VALUES(2,1,'test/data/sub_dir_extended_attribute',1569566935,1569148357,1569148357,1000,1000,16877);
INSERT INTO directories VALUES(3,1,'test/data/delta1',1569566935,1569148357,1569148357,1000,1000,16877);
INSERT INTO directories VALUES(4,1,'test/data/delta2',1569566935,1569148357,1569148357,1000,1000,16877);
INSERT INTO directories VALUES(5,1,'test/data/sub_dir',1569566935,1569148356,1569148356,1000,1000,16877);
CREATE TABLE links(  id              INTEGER PRIMARY KEY,  storageId       INTEGER,  name            TEXT,  destinationName TEXT,  timeLastAccess  INTEGER,  timeModified    INTEGER,  timeLastChanged INTEGER,  userId          INTEGER,  groupId         INTEGER,  permission      INTEGER,  FOREIGN KEY(storageId) REFERENCES storage(id));
INSERT INTO links VALUES(1,1,'test/data/link_not_existing_file.dat','not-existing-file',1569566934,1569148356,1569148356,1000,1000,41471);
INSERT INTO links VALUES(2,1,'test/data/circular_link.dat','circular_link.dat',1569569126,1569148356,1569148356,1000,1000,41471);
INSERT INTO links VALUES(3,1,'test/data/link.dat','linkdata.dat',1569566934,1569148356,1569148356,1000,1000,41471);
CREATE TABLE hardlinks(  id              INTEGER PRIMARY KEY,  storageId       INTEGER,  name            TEXT,  size            INTEGER,  timeLastAccess  INTEGER,  timeModified    INTEGER,  timeLastChanged INTEGER,  userId          INTEGER,  groupId         INTEGER,  permission      INTEGER,  fragmentOffset  INTEGER,  fragmentSize    INTEGER,  FOREIGN KEY(storageId) REFERENCES storage(id));
CREATE TABLE special(  id              INTEGER PRIMARY KEY,  storageId       INTEGER,  name            TEXT,  specialType     INTEGER,  timeLastAccess  INTEGER,  timeModified    INTEGER,  timeLastChanged INTEGER,  userId          INTEGER,  groupId         INTEGER,  permission      INTEGER,  major           INTEGER,  minor           INTEGER,  FOREIGN KEY(storageId) REFERENCES storage(id));
INSERT INTO special VALUES(1,1,'test/data/fifo',2,1569148357,1569148357,1569148357,1000,1000,4532,0,0);
COMMIT;
