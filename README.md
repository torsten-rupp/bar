# BAR
Backup ARchiver

BAR is backup archiver program. It can create compressed, encrypted
and split archives of files and disk images which can be stored
on a hard disk, cd, dvd, bd or directly on a server via ftp, scp,
sftp, webDAV/webDAVs, or SMB/CIFS. BAR can create full and
incremental/differential archives. A server-mode and a scheduler is
integrated for making automated backups in the background.

Features

* create, list, test, compare, convert and extract archives of
  files and disk images,
* fast access to entries in archives: can find and extract single
  files without decompressing/decryption of the whole archive,
* full and incremental/differential backup files archives,
* support for raw and several file systems for disk images (ext,
  fat, reiserfs),
* can split archives into parts of selectable size; each part can
  be read independent,
* compress of data with zlib, bzip2, lzma, lzo, lz4, or zstd
  algorithms,
* encrypt archive content with gcrypt algorithms (BLOWFISH,
  TWOFISH, AES, a. o.),
* asymmetric encryption with RSA,
* create PAR2 checksum files,
* store archives directly on external server via ftp or scp,
  sftp, webDAV, webDAVs, SMB/CIFS,
* store archives on CD/DVD/BD and add error correction codes
  with the external tool dvdisaster,
  on a generic device,
* multicore support for compression and encryption,
* server mode with included scheduler for doing backups regularly.
  Controlling the server can be done via network connection
  (plain & TLS/SSL),
* optional internal database with information to all stored files
  and disk images,
* graphical front end for server to check server status, create
  jobs, start jobs and stop jobs.
