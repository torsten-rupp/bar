# norootforbuild

Name:          bar
Version:       0.20
Release:       0
Summary:       Backup ARchiver
Source:        http://www.kigen.de/projects/bar/bar-%{version}.tar.bz2
URL:           http://www.kigen.de/projects/bar/index.html
Group:         Tool
License:       GPL-2.0
BuildRoot:     %{_tmppath}/build-%{name}-%{version}

BuildRequires: bc
BuildRequires: e2fsprogs
BuildRequires: gcc gcc-c++ glibc-devel make
BuildRequires: java-1.6.0-openjdk-devel
BuildRequires: jre >= 1.6.0
BuildRequires: m4
BuildRequires: patch
BuildRequires: tar
BuildRequires: tcl
BuildRequires: unzip
BuildRequires: wget
BuildRequires: xz

Requires(preun): initscripts
Requires(postun): initscripts

%description
BAR is backup archiver program. It can create compressed, encrypted
and splitted archives of files and harddisk images which can be
stored on a harddisk, cd, dvd, bd or directly on a server via ftp,
scp, sftp or webdav. A server-mode and a scheduler is integrated for
creating automated backups in the background.

%prep
%setup -q

%build
mkdir packages
(
  cp %{_sourcedir}/icu4c-*.tgz packages
  (cd packages; tar xzf icu*.tgz)
  ln -s `find packages -type d -name 'icu'|head -1` icu
)
(
  cp %{_sourcedir}/zlib-*.tar.gz packages
  (cd packages; tar xzf zlib-*.tar.gz)
  ln -s `find packages -type d -name 'zlib-*'|head -1` zlib
)
(
  cp %{_sourcedir}/bzip2-*.tar.gz packages
  (cd packages; tar xzf bzip2-*.tar.gz)
  ln -s `find packages -type d -name 'bzip2-*'|head -1` bzip2
)
(
  cp %{_sourcedir}/xz-*.tar.gz packages
  (cd packages; tar xzf xz-*.tar.gz)
  ln -s `find packages -type d -name 'xz-*'|head -1` xz
)
(
  cp %{_sourcedir}/lzo-*.tar.gz packages
  (cd packages; tar xzf lzo-*.tar.gz)
  ln -s `find packages -type d -name 'lzo-*'|head -1` lzo
)
(
  cp %{_sourcedir}/lz4-*.tar.gz packages
  (cd packages; tar xzf lz4-*.tar.gz)
  ln -s `find packages -type d -name 'lz4-*'|head -1` lz4
)
(
  cp %{_sourcedir}/xdelta3-*.tar.gz packages
  (cd packages; tar xzf xdelta3-*.tar.gz)
  ln -s `find packages -type d -name 'xdelta3-*'|head -1` xdelta3
  (cd xdelta3; patch --batch -N -p1 < ../../misc/xdelta3-3.1.0.patch)
)
(
  cp %{_sourcedir}/libgpg-error-*.tar.bz2 packages
  cp %{_sourcedir}/libgcrypt-*.tar.bz2 packages
  (cd packages; tar xjf libgpg-error-*.tar.bz2)
  (cd packages; tar xjf libgcrypt-*.tar.bz2)
  ln -s `find packages -type d -name 'libgpg-error-*'|head -1` libgpg-error
  ln -s `find packages -type d -name 'libgcrypt-*'|head -1` libgcrypt
)
(
  cp %{_sourcedir}/openssl-*.tar.gz packages
  (cd packages; tar xzf openssl-*.tar.gz)
  ln -s `find packages -type d -name 'openssl-*'|head -1` openssl
)
(
  cp %{_sourcedir}/c-ares-*.tar.gz packages
  (cd packages; tar xzf c-ares-*.tar.gz)
  ln -s `find packages -type d -name 'c-ares-*'|head -1` c-ares

  cp %{_sourcedir}/curl-*.tar.bz2 packages
  (cd packages; tar xjf curl-*.tar.bz2)
  ln -s `find packages -type d -name 'curl-*'|head -1` curl

  cp %{_sourcedir}/mxml-*.tar.gz packages
  (cd packages; tar xzf mxml-*.tar.gz)
  ln -s `find packages -type d -name 'mxml-*'|head -1` mxml
)
(
  cp %{_sourcedir}/libssh2-*.tar.gz packages
  (cd packages; tar xzf libssh2-*.tar.gz)
  ln -s `find packages -type d -name 'libssh2-*'|head -1` libssh2
)
(
  cp %{_sourcedir}/nettle-*.tar.gz packages
  (cd packages; tar xzf nettle-*.tar.gz)
  ln -s `find packages -type d -name 'nettle-*'|head -1` nettle

  cp %{_sourcedir}/gmp-*.tar.bz2 packages
  (cd packages; tar xjf gmp-*.tar.bz2)
  ln -s `find packages -type d -name 'gmp-*'|head -1` gmp

  cp %{_sourcedir}/gnutls-*.tar.xz packages
  (cd packages; xz -d -c gnutls-*.tar.xz | tar xf -)
  ln -s `find packages -type d -name 'gnutls-*'|head -1` gnutls
)
(
  cp %{_sourcedir}/libcdio-*.tar.gz packages
  (cd packages; tar xzf libcdio-*.tar.gz)
  ln -s `find packages -type d -name 'libcdio-*'|head -1` libcdio
)
(
  cp %{_sourcedir}/pcre-*.tar.bz2 packages
  (cd packages; tar xjf pcre-*.tar.bz2)
  ln -s `find packages -type d -name 'pcre-*'|head -1` pcre
)
(
  cp %{_sourcedir}/binutils-*.tar.bz2 packages
  (cd packages; tar xjf binutils-*.tar.bz2)
  ln -s `find packages -type d -name 'binutils-*'|head -1` binutils
)
(
  cp %{_sourcedir}/breakpad.tar.bz2 packages
  (cd packages; tar xjf breakpad.tar.bz2)
  ln -s `find packages -type d -name 'breakpad'|head -1` breakpad
)

%configure --enable-package-check
%{__make} OPTFLAGS="%{optflags}"

%install
%makeinstall DESTDIR=%{buildroot} DIST=1 SYSTEM=CentOS

%clean
%__rm -rf "%{buildroot}"

%check
%{__make} test1 test2 test3 test5 COMPRESS_NAMES_LZMA="lzma1 lzma2 lzma3 lzma4 lzma5 lzma6 lzma7"

%pre

%post
service barserver start 1>/dev/null

%preun
service barserver stop 1>/dev/null

%postun

%files

%defattr(-,root,root)

%{_bindir}/bar
%{_bindir}/bar-debug
%{_bindir}/bar-debug.sym
%{_bindir}/barcontrol
%{_bindir}/barcontrol-linux.jar
%{_bindir}/barcontrol-linux_64.jar
%{_bindir}/bar-keygen
%{_bindir}/bar-sqlite3
/etc/init.d/barserver

%dir /etc/bar
%dir %attr(0700,root,root) /etc/bar/jobs

%config(noreplace) %attr(0600,root,root) /etc/bar/bar.cfg
%config(noreplace) %attr(0600,root,root) /etc/logrotate.d/bar

%lang(de) %dir %{_datadir}/locale/jp
%lang(jp) %dir %{_datadir}/locale/jp/LC_MESSAGES
%lang(de) %{_datadir}/locale/de/LC_MESSAGES/%{name}.mo
%lang(jp) %{_datadir}/locale/jp/LC_MESSAGES/%{name}.mo

%doc ChangeLog doc/README
%doc doc/bar.pdf
%doc %{_mandir}/man7/bar.7.gz

%changelog
* Thu Oct 13 2016 Torsten Rupp <torsten.rupp@gmx.net> 0.20
  - fixed max. size for LZO/LZ4 compression when block cannot
    be compressed
  - TLS port is now optional: if possible as SSL protected
    connection is establed via the standard plain port, too.
  - added option --archive-file-mode: stop, append or
    override; removed configurationm option
    overwrite-archive-files (deprecated)
  - delete empty directories when purge old storage files
  - BARControl: file requester with remote file list
    local file requester with CTRL+click
  - BARControl: added support to connect to different servers;
    added connect menu
  - BARControl: added unit TByte/TB
  - added option --log-format for log date format, standard
    log date format is now YYYY-MM-DD hh:mm:ss
  - fixed clean-up duplicate database entries: delete
    storage index
  - improved delete database entries
  - separate log for each executed job
  - removed macro %%last for archive names
  - support full-text-search in database
  - BARControl: removed "connector" button in restore tab;
    search is automatically filtered by selected entities,
  - BARControl: new restore dialog with list, destination
    and directory content option
  - Added test button for scripts
  - added option --mount: mount/unmount devices before/after
    execution of job
  - deprecated option --mount-device
  - added option --include-command, --exclude-command
  - added configuration options include-file-command,
    include-image-command, exclude-command
  - add job option comment: free text comment
  - added code-coverage analysis to build process
  - improved database access
  - moved database file /usr/lib/bar -> /var/lib/bar
  - change option: --stop-on-error -> --no-stop-on-error,
    --stop-on-error is now deprecated
  - webDAV: fixed race condition in receive data
  - create temporary log file in system temporary directory
  - upgrade libssh2: 1.7.0
  - fixed possible dead-lock when a specific error
    occurred while executing a job
  - fixed possible wrong error text
  - improved parsing configuration files: reject unknown
    values
  - fixed init script on CentOS
  - added option --server-max-connections: limit max.
    number of concurrent server connections, default 8
  - BARControl: added option --force-ssl
  - Upgrade bzip2 to 1.0.6
  - added bar-sqlite3 tool
  - added logrotate script, support for log rotate
  - fixed auto-index: search for .bar files
  - BARControl: fixed login credentials on command line
  - BARControl: fixed archive name editor %%S, %%s, _
  - fixed storage of splitted hardlink-entries!
  - fixed abort when there is an error when writing
    a CD/DVD/BD
  - added options --blank, --(cd|dvd|bd|device)-blank to
    blank medium before writing
  - BARControl: added abort to load volume dialog
  - BARControl: renamed option --index-database-storage-list
    -> --index-database-storages-list, added option
    --index-database-entities-list  
  - fixed crash with multiple connections from same host
    and authetification failure

* Sat Jan 09 2016 Torsten Rupp <torsten.rupp@gmx.net> 0.19d
  - fixed include of multiple entries with pattern: store
  - and foo/b.* if exists
  - fixed possible crash when files with %% in name may not
    be readable
  - upgrade PCRE 8.38
  - BARControl: fixed typing errors in translation
  - fixed memory leak

* Sun Dec 06 2015 Torsten Rupp <torsten.rupp@gmx.net> 0.19c
  - added user, group, permission to long list output
  - fix get user/group name with large number of names
  - BARControl: fixed parsing float number with different
    locales
  - BARControl: fixed edit pre/post-scripts on Windows:
    replace line ending CRLR by LF.
  - BARControl: improved restore dialog: show failed
    entries, show total progress bar
  - fixed assert in restore archives
  - BARControl: fixed restore complete archives

* Sun Nov 22 2015 Torsten Rupp <torsten.rupp@gmx.net> 0.19b
  - fixed reading job files with none-LF at end
  - try to delete temporary directory also on Ctrl-C or
    a crash
  - fixed index auto-update: search for .bar files in
    directory and all sub-directories
  - BARControl: fixed possible null-pointer-exception
    on Windows
  - BARControl: fixed layout buttons
  - BARControl: fixed translation
  - BARControl: fixed multi language support (language files
    were missing in distribution)
  - fixed scanning for .bar-archives: scan in sub-directories,
    too
  - BARControl: fixed typing error in help
  - BARControl: fixed null-pointer-exception when delete
    entity in restore tab
  - fixed init scripts

* Sat Oct 31 2015 Torsten Rupp <torsten.rupp@gmx.net> 0.19a
  - changed upgrade database: do not lock database
  - added support to output stacktraces! Press Ctrl-\
    or send signal SIGQUIT to BAR
  - improved error handling in database
  - always unmount mounted device if init storage fail
  - fixed typing error in scp protocol init
  - fixed init SSH default password
  - fixed some compilation errors when packages are
    disabled
  - added barserver.service for systemd
  - fixed init.d-start script for Fedora
  - fixed install init-scripts
  - fixed --quiet option
  - upgraded libgpg-error to 1.20: fixed problem with
    pre-processor and newer gcc versions
  - upgraded libgcrypt to 1.6.4: fixed problem with
    pre-processor and newer gcc versions
  - improved error message when ssh login fail
  - fixed possible dead-lock in server when file
    .nobackup cannot be created
  - improved error message when SSL authentification
    fail
  - fixed compiling ring buffer when no backtrace()
    function is available
  - removed wrong assert in server.c
  - fixed compiling without ulong
  - fixed unintialized data with unknown job UUIDs
  - fixed unintialized data in database access
  - BARControl: fixed number format exception when BAR
    and BARControl use different locales
  - fixed memory corruption when stringmaps are
    enlarged
  - fixed error message for wrong double config values
  - fixed exitcode list archives
  - fixed possible deadlock in server when started as
    a daemon
  - fixed possible crash when index database could not
    be opened or is created
  - BARControl: fixed output of --list
  - fixed possible crash when decompress broken archive
    compressed with LZO
  - upgraded LZO to 2.09
  - use LZO safe-decompress, fixed valgrind warning
  - disabled test Serpient for CentOS 5: do not use!
    Probably bug in gcc 4.1 together with -O2; solved
    in newer versions of gcc.

* Fri May 01 2015 Torsten Rupp <torsten.rupp@gmx.net> 0.19
  - added LZO compression
  - added LZ4 compression
  - fixed large file support: on some systems large files
    (>4GB) could be incompletely written
  - do not modify atime of files when creating or compare
    archives (NO_ATIME)
  - BARControl: added new/clone/rename/delete to status tab
    option menu
  - added pre-/post-command
  - improved crash reporting: include symbol file
  - improved logging
  - BARControl: added different exclude types in
    context menu (nodump, .nobackup, exclude)
  - BARControl: added pre-/post scripts
  - BARControl: added archive tree view to restore tab
  - added "entities" for set of created storage files:
    keep set of storage in the internal database and show
    created backups in a tree view in BARControl
  - BARControl: added function in restore tab context
    menu to assign storage to some entity
  - started localization support (English, German,
    and Japanese! Thanks to Satoko Koiwa)
  - added no-storage to schedule: schedule just to create
    .bid file
  - added min/max/age to schedule
  - BARControl: show tooltip in restore tab in left
    half of tree/table
  - BARControl: show number of entries in entity and
    storage (tooltip info)
  - BARControl: add function in context menu to trigger
    a specific schedule entry
  - BARControl: improved statistic view of no-storage
    job
  - upgraded GMP to version 6.0.0a: fix usage of wrong
    code on AMD architecture.
    Thanks to Shane for his help to understand this bug!
  - BARControl: added option --version
  - fixed parsing compress algorithm names
  - improved error when reading encryption algorithm from
    archives
  - fixed matching files with \ in name (glob pattern)
  - improved tests
  - fixed delete files via webdav-protocol
  - fixed error if entered passwords are not equal
  - fixed listing, testing of content in directory:
    support pattern matching
  - fixed possible infinite recursion when processing
    delta compressed files while searching for
    delta sources
  - improved error output for libcdio
  - fixed parsing schedule data
  - BARControl: fixed schedule dialog, removed "*" archive
    type
  - fixed race-condition in init database (thanks to Shane)
  - fixed restore: keep root (first /) when no directories
    are stripped
  - fixed sending data via scp: close and transmit last
    data package
  - fixed superfluous output on stdout of ftp+webdav
    protocol

* Mon Nov 10 2014 Torsten Rupp <torsten.rupp@gmx.net> 0.18e
  - removed nice-level in init scripts
  - BARControl: fixed selection and storage of
    file names via file dialog
  - BARControl: fixed restore destination directory
    bug + overwrite flag

* Mon Sep 29 2014 Torsten Rupp <torsten.rupp@gmx.net> 0.18d
  - fix not closed file when --no-storage is specified

* Fri Sep 26 2014 Torsten Rupp <torsten.rupp@gmx.net> 0.18c
  - fixed server network connection wait time in pselect();
    may caused network trouble on some Linux systems.
    Thanks to Daniel Webb for his help to find this bug!
  - BARControl: upgraded to SWT 4.4 to fix a crash on
    close when a combo widget is disposed.
    (https://bugs.eclipse.org/bugs/show_bug.cgi?id=372560)
    Thanks to Daniel Webb for his help to find this bug!
  - Upgraded LZMA library to version 5.0.7

* Fri May 09 2014 Torsten Rupp <torsten.rupp@gmx.net> 0.18b
  - fixed server start script BAR server for Debian
  - do not download epm, breakpad by default
  - fix compilation some third party package: m4 required

* Thu May 01 2014 Torsten Rupp <torsten.rupp@gmx.net> 0.18a
  - upgraded OpenSSL to 1.0.1g (fix for Heartbleed bug)
  - fixed replaced storage archives in database
  - BARControl: fixed possible exception when session id key
    data cannot be parsed (NumberFormatException)
  - BARControl: fix delete database index entries
  - BARControl: fixed possible exception when insert
    new element into exclude list, compress exclude list
  - fix compile non-debug version without stacktrace function
  - install BAR binary in /usr/bin
  - fixed install of man-page
  - fixed include bar-keygen in base package
  - fixed man page
  - BARControl: improved error message when a SSL connection
    cannot be established
  - BARControl: fixed error handling
  - fixed free resources when creating image archives

* Fri Mar 21 2014 Torsten Rupp <torsten.rupp@gmx.net> 0.18
  - added multi-core compression/encryption support!
  - dramatic speed-up when creating archives!
  - added automatic delete of duplicate index entries when
    server is started
  - store and restore extended file attributes (EAs)
  - improved FTP access: replaced FTPLib by curl+c-areas
  - added support for WebDAV protocol
  - added support for crash dump tool Breakpad
  - integrated a scan-mode which will scan partially broken
    archives for still readable parts
  - server protocol changes: replaced list of values by value map
    for easier extension of protocol. Note: you _must_ update
    both server and clients with this change.
  - added uuid to jobs
  - added custom text to schedule entries
  - added macros %%uuid, %%text to storage name template
  - added options --[ftp|ssh|webdav]-max-connections: limit
    number of concurrent network connections for server
  - BARControl: added time macros %%U2, %%U4, %%W, %%W2 and %%W4,
    added macros %%T, %%uuid, %%text
  - BARControl: added file selector button to storage name
    input
  - support listing remote directory content
  - start support cross-compilation Linux -> Windows
  - upgrade gpg-error 1.10, gcrypt 1.5.0, gnutls 3.1.18, libcdio 0.92,
    pcre 8.34
  - fixed create index: do not delete index which is currently
    updated
  - fixed output in verbose/quiet mode
  - fixed restore: list files when "only newest" is disabled
  - fixed duplicate archive entries when multiple include options
    are specified (thanks to Stefan A.)
  - fixed writing .bid file when no directory is given
  - BARControl: fixed duplicate job
  - BARControl: added insert/edit/delete keyboard shortcuts
  - BARControl: passwords sent to the BAR daemon are now RSA
    encrypted! This makes it much harder to steal plain text
    passwords even when no TLS encrypted network connection is
    used.
  - improved tests: added special testcode to execute error
    handling code in tests
  - using gcc to compile is now mandatory (required because of
    improved error-handling with using closure like code)
  - improved defense against denial-of-service-attack: force
    delay for clients with multiple authorization failure
  - changed schedule entry in job configuration: use a section.
    Entries in the old format are still read, but not created
    anymore.
  - avoid console input in non-interactive mode
  - fixed BARControl: list storage ignoring case
  - fixed BARControl: delete storage files

* Sun May 05 2013 Torsten Rupp <torsten.rupp@gmx.net> 0.17b
  - fixed parsing of storage specifier (ftp, scp/sftp,
    cd/dvd/bd): user name
  - fixed dialog text when password is requested
  - improved checking host name for ftp/scp/sftp login
  - fixed lost string resource

* Sat Jan 26 2013 Torsten Rupp <torsten.rupp@gmx.net> 0.17a
  - fixed writing CD/DVD/BD: do not write an empty last medium
  - improved database index
  - fixed parsing CD/DVD/BD/device specifier: last character was
    missing
  - fixed logging of skipped own files

* Wed Oct 03 2012 Torsten Rupp <torsten.rupp@gmx.net> 0.17
  - finally: added support for xdelta compression!
    Note: integrating this was really a _hard_ work.
  - added support for libcdio to read content of CD/DVD/BD
    devices/images without mounting
  - upgraded used libssh2 to version 1.4.2
  - use libssh2 send64() when available to be able to
    send large files, too
  - added check for file permission of config files. Should
    be 400 or 600.
  - improved error handling with public/private keys
  - fixed handling of error case when asymmetric encrypted
    archive cannot be read
  - print fragment info if file is incomplete on verbose
    level >= 2
  - fix header information in sources
  - improved logging for not stored files
  - support no-dump file attribute, add option
    --ignore-no-dump (see lsattr, chattr)
  - set default ssh keys to $HOME/.ssh/id_rsa.pub,
    $HOME/.ssh/id_rsa
  - fix string free bug in restore
  - fix file-seek bug in restore
  - BARControl: add warning when minor protocol version do
    not match
  - BARControl: ask for FTP/SSH password on restore if
    required
  - added internal debug code for list allocations
  - added hidden option --server-debug for automated test
    of server functions
  - improved valgrind tests
  - output percentage info if verbose level >= 2
  - output test/compare/restore info if verbose level >= 1
  - improved processing speed: implemented ring buffers
    for compress/decompress
  - fixed compare of images: only compare used blocks
  - fixed creating FAT images: in some cases one block was
    missing in the archive
  - improved tests: added more tests for images
  - stop support of Reiser 4: file system is not supported
    anymore by all Linux versions
  - if an encryption password is specified on the command
    line do not ask for another password if decryption fail
  - clean-up of BAR manual
  - improved console output with multiple threads: avoid
    mixing output of lines in interactive mode
  - renamed command line option --database-file -> --database-index
  - renamed command line option --no-auto-update-database-index
    -> --database-index-auto-update
  - renamed config option database-file -> index-database
  - renamed config option no-auto-update-database-index
    -> index-database-auto-update
    WARNING: modify your bar.cfg file!
  - added command line/config option --datbase-index-keep-time
  - set example for log-post-command to (removed quotes):
    sh -c 'cat %%file|mail -s "Backup log" root'
  - fix parsing configuration files: strings must not be quoted
    explicitly
  - improved error output when log-post command cannot be executed:
    show last 5 lines of stderr
  - fixed usage of libcrypt: enabled multi-thread-support
  - set read-timeout for SSH connections to avoid possible infinite
    blocking in libssh2-code when remote side may close socket
    unexpected
  - BARControl: add ellipsis character to menus/buttons which
    require further user input
  - BARControl: added schedule copy menu entry/button
  - BARControl: show tool tip in tree view only when mouse is
    in the left side to avoid that tooltip is shown all the
    time
  - BARControl: add functions to clear stored passwords on
    server
  - BARControl: renamed option --index-add -> --index-database-add,
    renamed option --index-remove -> --index-database-move
  - BARControl: renamed config option pause-index-update ->
    pause-index-database-update
  - fixed possible crash in log file post processor
  - BARControl: fix abort job. Set last executed time.
  - BARControl: fix parsing storage name when login name is not given
  - BARControl: fixed cloning of schedule entries
  - BARControl: added path selector in include/exclude dialogs
  - BARControl: added confirmation dialog when include/exclude entry
    should be removed
  - BARControl: fixed schedule hour setting, show 00..23
  - BARControl: added function to delete storage files in context menu
    in restore tab
  - added configuration option index-database-max-band-width: limit the
    used band width for background index updates
  - improved band width limitation: specify either a value or name of an
    external file
  - fixed scheduler: do not start job immediately again when execution
    time was longer than time period of scheduling
  - added optional time range to max-band-width,
    index-database-max-band-width options to support different limits
    depending on date/time
  - support external file for max-band-width,
    index-database-max-band-width options
  - fixed memory leak in server

* Sat Jan 14 2012 Torsten Rupp <torsten.rupp@gmx.net> 0.16g
  - fix restore: do not create empty parent directories
  - fix restoring archive entries without directory part
  - added verbose level 5, output ssh debug messages for
    level 4 and 5
  - fixed crash when archive on CD/DVD/BD/device should
    be listed/tested/extracted directly. Please mount
    a CD/DVD/BD/device and use the file operations instead.
    Next version of BAR will support reading CD/DVD/BD
    directly
  - fixed memory leaks

* Sat Sep 10 2011 Torsten Rupp <torsten.rupp@gmx.net> 0.16f
  - re-added option --volume-size, fixed setting CD/DVD/BD
    volume size
  - added check for Java version
  - improved error messages
  - do not print passwords which may be included in FTP
    specifier when creating/list/restore entries
  - BARControl: ask for crypt password when restoring
    single archive entries
  - fixed URI parser: clear port number if not set
  - improved index database: do not set error state when
    connection to server cannot be opened
  - fixed memory leaks
  - back-ported file handle debug code
  - do not set index state to error if archive cannot be
    decrypted because of missing password
  - fix bug in ftplib: listing directory did not close
    temporary file

* Sun Jul 31 2011 Torsten Rupp <torsten.rupp@gmx.net> 0.16e
  - create sub-directories when storing to file system or
    ftp server
  - fix creating directories: to not set parent directory
    permissions if permissions are already set
  - BARControl: do not close new job dialog on error
  - BARControl: fixed setting crypt type radio buttons with
    default value
  - save job file immediately after creating a new one
  - fix lost error state when writing file to a server
  - improved FTP transmission, added patch to set timeout
    for receiving data in FTPLib
  - fixed possible infinite loop when executing external
    command, e. g. sending the log file via log-post-command
  - fixed error in pre-defined log-post-command in bar.cfg
  - fix losing failure error
  - create job files with read/write permission for owner only
  - do not print passwords to log/screen which may be included
    in FTP specifier
  - fixed missing delete temporary file on error in incremental
    mode
  - fixed adding index of FTP content to local archive database
  - fixed string parser: parse \x as x if outside of " or '
  - BARControl: fixed null-pointer-exception  when server password
    is not set
  - improved FTP read: try to read all data when the network
    connection is bad
  - fix CD/DVD/BD write command: must be %%directory, not %%file
  - BARControl: add command to remove all archives with error
    state in tab restore
  - BARControl: fixed possible null-pointer-exception on communication
    error
  - BARControl: improved usability. Use tagged+selected entries
    entries to remove/refresh  in tab restore.
  - add log type "index"
  - BARControl: fixed list of storage archives when removing an
    entry
  - BARControl: ask for crypt password when starting job with
    crypt password mode "ask"
  - BARControl: give visual feedback when entered passwords in
    password dialog are not equal
  - show date/time for directories entries in list, too
  - changed DVD write image command: removed sectors, added
    -dvd-compat
  - output stdout/stderr of external commands with verbose level 4
  - BARControl: added some warnings when selection of part size/
    medium size/error correction codes may not fit to create a
    CD/DVD/BD

* Tue May 31 2011 Torsten Rupp <torsten.rupp@gmx.net> 0.16d
  - BARControl: fix parsing of ftp/scp/sftp archive names
  - fix parsing of ftp archive names
  - support for non-passive/passive ftp connections
  - fixed wrong free of resources when archive entry could
    not be read
  - fixed numbers for compress/crypt type: must be a real
    constant (self-assigned enum may change)

* Sun Apr 17 2011 Torsten Rupp <torsten.rupp@gmx.net> 0.16c
  - renamed macro %%file -> %%directory for cd/dvd/bd/device-commands
  - fixed typing error in code when FTP is not available
  - added options --file-write-pre|post-command,
    --ftp-write-pre|post-command, --scp-write-pre|post-command,
    --sftp-write-pre|post-command
  - added option --always-create-image
  - fixed creating CDs: use mkisofs+cdrecord
  - removed double linefeed for log entries
  - fixed log entries when creating database index: avoid
    creating huge log files
  - only write log file in daemon/server mode
  - add missing MacOSX JARs to distribution
  - renewed man page, improved manual

* Sun Apr 10 2011 Torsten Rupp <torsten.rupp@gmx.net> 0.16b
  - fix error handling when password is wrong
  - create jobs directory if it does not exists
  - enable creating index database by default
  - BARControl: fix file name editor drag+drop
  - BARControl: ask for password on restore
  - fix memory leak when reading archive directory entries
  - fixed deinit error when password is wrong
  - fixed reading file names which contain \ or LF/CR
  - BARControl: fix discarding first character for archive target cd:, bd:
  - BARControl: fixed file listing
  - fix error handling when calling external tool for CD/DVD/BD
  - BARControl: add year two digits in archive file name editor, fixed
    century
  - BARControl: fix enabling restore-button
  - improved documentation: added archive file name macros, add more
    entries to FAQ, small fixes

* Thu Dec 30 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.16a
  - fix wrong storage name in database
  - BARControl: fix start/abort button enable/disable for
    incremental/differential/dry-run jobs
  - BARControl: fix tab restore storage filter: edit+reset
  - clean-up

* Fri Dec 17 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.16
  - do not store content of directories and sub-directories
    when file .nobackup or .NOBACKUP exists
  - add option --ignore-no-backup-file
  - support hard links: add new chunk types HLN0, HENT, HNAM,
    HDAT
  - rewrite chunk code: add init and done, moved clean-up
    code down to chunk code, improve error handling
  - fix wrong data parsing in index database for directories,
    links, special files
  - improved error handling
  - fix typing errors in error messages
  - improved tests
  - added -o as shortcut for --overwrite-archive-files
  - added option --dry-run: do all operations, but do not
    compress, encrypt and store files, do not write incremental
    data lists, do not write on CD/DVD/BD/devices
  - fixed --no-storage: incremental data list was not written
  - fixed possible deadlock in server when archive files cannot
    be transmitted to a remote server and local hard disk becomes
    full
  - added option --compress-exclude to disable compression for
    files, images and hard links which match to the specified
    pattern
  - BARControl: added exclude list in storage tab under compress
  - fixed memory leak
  - support building Debian/Ubuntu packages with SuSE build
    service!
  - fix install: add bar-keygen
  - added option --differential: differential storage. Like
    --incremental, but incremental data is not updated
  - fixed creating parent directories when restoring files:
    use default file mask to create parent directories
  - fix restoring splitted files which are read only
  - fixed typing error in bar.cfg entry "schedule": must be Apr,
    not Arp. Note: please fix this in your jobs files in /etc/bar/jobs,
    too, when you created a job which should be scheduled in April!
  - fixed SigSegV with option -g: internal sorting of list was wrong

* Sun Dec 05 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.15f
  - fix SigSegV in string.c:formatString() which can occur on
    AMD 64bit systems

* Thu Dec 02 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.15e
  - BARControl: fix exception when running BARControl under
    Windows and connecting to a Linux server (file separator
    is different on Windows and Linux; this is a temporary fix
    which will be improved in version 0.16)
  - BARControl: made columns size, modified, state in restore
    tab, storage list resizable, too
  - fixed wrong BAR binary path in SuSE, Redhat, Mandrake, Fedora
    start scripts
  - fixed wrong BAR config path in SuSE, Redhat, Mandrake, Fedora
    start scripts
  - BARControl: fixed some table column widths (automatic setting
    seems to be different on Windows)
  - BARControl: fix arguments in start script BARControl.bat

* Mon Nov 29 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.15d
  - fix SigSegV when deleting a storage archive from
    the database via BARControl

* Thu Nov 18 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.15c
  - fixes for MacOSX:
    - fix SWT start thread problem
    - use "java" without any path
    - fix out-dated JAR archives
  - added missing documentation images to distribution
  - fix creation of BARControl/BARControl*.jar
  - fix error message when archive file already exists

* Wed Nov 17 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.15b
  - BARControl: fix sorting of columns
  - BARControl: fix not working context menu "exclude", "none" in
    status tab, files list, removed debug code
  - fix archive file name for CD/DVD/BD
  - BARControl: fix sorting of columns in jobs tab file/device tree
  - BARControl: fix calculating directory size (context menu in
    file list in jobs tab)
  - fixed parsing of storage specification for cd/dvd/bd

* Tue Oct 26 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.15a
  - fix compilation problems with LONG_LONG_MAX
  - fix typecast in strings.c for 64bit systems
  - fix man create install path
  - support build service (thanks to lalalu42)
  - add make variable DIST, SYSTEM to install
  - update download-script: gnutls 2.10.2
  - fix typing error in strings debug code
  - use getpwnam_r and getgrnam_r to avoid multi-threaded problems
  - fixed wrong locking code in semaphore read-lock
  - fixed possible infinite blocking in index update (ssh read)
  - fixed memory leak in index thread
  - fixed test for large file support on 64bit systems
  - fixed ssh-communication problem ("bad record mac" in BARControl),
    improved error messages
  - added different BAR daemon start scripts, fixed start script
  - enable dynamic linkage of system internal libraries
  - clean-up Makefile file

* Wed Oct 13 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.15
  - added command line option --job to execute a job from a job
    file with BAR
  - clean-up BAR command line options:
    - replaced -a|--crypt-asymmetric -> --crypt-type=<name>
    - added option --normal to select normal archive type
    (required to overwrite setting in job file)
    - fixed ordering of options in help
  - added support for CD and BD (options --cd-.../--bd-... and configuration
    entries cd-.../bd-...)
  - clean-up BARControl command line options:
    renamed --job-mode -> --archive-type
  - fixed name of option --dvd-write-image-command
  - BARControl: fix option --key-file
  - renamed command line option --no-bar-on-dvd -> --no-bar-on-medium and
    configuration file entry no-bar-on-dvd -> no-bar-on-medium
  - set key valid time to 365 days when created with openssl
  - BARControl: added tool tips help
  - BARControl: complete redesign of restore tab
    - added database of stored files to BAR (sqlite based)
    - search for created archives in database
    - search for stored files in archives in database
    - restore archives or single files
    - automatic indexing of already created archives which are stored
    in file system or on an external server
    Note: to use this new feature, you must add the database-*
    configuration options in bar.cfg!
  - temporary base directory is now named bar-XXXXXX
  - BARControl: improved pause-function: settings for create, storage,
    restore and update index. See menu in BARControl. By default create
    and restored are paused only
  - BARControl: add destination types "cd" and "bd", add different images
    sizes for cd/bd
  - BARControl: clean-up layout, rearranged some buttons
  - BARControl: updated to SWT 3.6.1
  - create package bar-gui-*.zip with compiled GUI only
  - documentation: renewed screen shots, added documentation of
    new options, fixed documentation of some options, clean-up
  - updated man-page

* Wed Jul 28 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.14
  - added command line control functions to BARControl:
    --list, --job, --job-mode, --abort, --ping, --pause,
    --suspend, --continue
  - improved BARControl command line parser
  - BARControl: added file selector buttons for ssh keys, device name
  - fixed BARControl restore tab: listing path names
  - fix C string parser: %%s and %%S can be empty strings
  - BARControl: fixed abort in restore dialog, some clean-up. Note:
    The restore tab will be improved in some of the next releases.
  - BARControl: fixed parsing of archive part size (job was ignored
    when number was bigger than 32bit; now 63bit are allowed)
  - fixed display of archive/device sizes: units are G,M,K
  - add warning when no BAR server password given
  - BAR server: added support to list image entries
  - updated manual
  - clean-up

* Sat Jul 24 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.13d
  - fix wrong installation path "/man/man7" man pages: now
    /usr/share/man/man7

* Fri Jun 11 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.13c
  - fix bar-keygen which caused broken Debian package: wrong template
    file names
  - fix bar-keygen: create bar.jsk in /etc/bar by default
  - improved bar-keygen: check if keys exists, add option --force
  - improved BAR server: additional check /etc/ssl/private for
    bar.jks
  - fix some typing errors in manual
  - fix Makefile of BARControl: SWT JAR version

* Sun May 23 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.13b
  - fix assert-error in options -#/-!
  - improved test of file functions (added -#, -!)
  - fix on-the-fly compiling of libgcrypt: use provided libgpg-error
    instead of system-libraries (which may not exists)
  - fixed and improved download of zlib: download recent version
  - small fixes in manual

* Sat Apr 10 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.13a
  - fix typing error in "owner"
  - fix wrong parsing of "owner" in server
  - fix memory leak in server
  - BARControl: fix opening directories in file tree
  - added epm to download third-party script

* Sun Mar 21 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.13
  - implemented device image functions: create images from devices
  - implemented support for images with ext2/ext3, fat12/fat16/fat32
    file systems
  - renamed option --directory to --destination, use it for
    destination of images, too
    IMPORTANT: please edit your files in /etc/bar/jobs and
    replaced "directory = ..." by "destination = ..."!
  - verify if server TLS certificate expired; output error
    message
  - fix halt on some not implemented functions when
    restoring files
  - added stack backtrace output in string-debug functions
  - fix lost string when directory cannot be created
  - added option --owner
  - BARControl: added support for 64bit systems
  - fixed some C compiler warnings
  - fixed creating directory when writing incremental file list
  - improved error messages
  - fixed memory leak
  - improved valgrind suppression rules
  - fixed compilation warnings
  - improved and clean-up command line/config value parsing
  - set socket timeout in BARControl (20s)
  - improved error messages in BARControl
  - updated SWT JAR to 3.6 (this fix a bug with GTK versions
    >= 2.18)
  - added 64bit SWT JAR for Windows
  - added SWT JAR for MacOSX (experimental)
  - fixed C stacktrace output in debug mode: print function names
  - added option -L = --long-format
  - added option --human-format, -H: print sizes in human readable
    format (size+unit)
  - added first version of a manual!
  - improved configure checks for doc tools

* Fri Jan 01 2010 Torsten Rupp <torsten.rupp@gmx.net> 0.12d
  - fixed wrong usage of vprintf in log function (caused SigSegV
    on 64bit systems)

* Tue Oct 20 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.12c
  - fixes in configure: fix large file support for some system
  - fixes in configure: add libcrypto only if available (required
    for some older libssh2 implementations)

* Sat Oct 17 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.12b
  - added option --owner to overwriting settings for
    user/group of restored files
  - fixed creating directories: use default user creation
    mask for parent directories
  - revert error to warning when permission/owner ship
    cannot be set if --stop-on-error is not given
  - fixed configure check for EPM: do not try to detect
    version if no EPM installed
  - fixed configure check for gcrypt version

* Tue Sep 15 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.12a
  - clean-up design of progress bars
  - fixed layout of part-editor (canvas widget)

* Sun Jul 19 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.12
  - disabled -fschedule-insns2 optimization. This cause with my
    gcc 3.3 a problem in String_parse() when a boolean value
    should be read. It seems the address of the variable become
    wrong when -fno-schedule-insns2 is not given (thus
    schedule-insns2 optimization is enabled). Is this a gcc
    bug?
  - added crypt password in jobs-storage-tab: password can be
    default (read from configuration file), interactive input
    (ask) or specified password
  - BARControl: added check if JDK key file is valid
  - BARControl: improved password dialog
  - BARControl: added password dialog when crypt password mode
    is "ask"
  - fixed statics of skipped/error files
  - BARControl; added pause button with timeout (default
    60min)
  - added support for lzma compression
  - added script for simple download of additional packages
  - added support to build additional packages when compiling
    BAR (extract packages to specific sub-directories or use
    links)
  - BARControl: added copy job button
  - BARControl: check if job exists for new, copy, rename

* Sun Apr 12 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.11b
  - BARControl: improved volume dialog: unload tray button
  - fixed handling of DVDs
  - BARControl: fixed progress BAR for volume
  - BARControl/server: fixed writing crypt-type entry (was empty)

* Tue Mar 31 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.11a
  - fixed accidently removed save/cancel button in
    storage part edit dialog
  - BARControl: fixed URI parsing
  - fixed string-error in DVD functions

* Sun Mar 22 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.11
  - added selection of multiple days to schedule days
    configuration, e. g. you can specify "Mon,Tue,Sat"
  - BARControl: fixed quit (internal threads blocked quit)
  - BARControl: changed names of buttons in file tree
  - BARControl: added button to open all included directories
    in file tree list (green directory symbol at the bottom
    right side)
  - BARControl: added function to detect sizes of directories;
    when "directory info" checkbox is enabled and a
    sub-directory is opened the sizes of the directories are
    detected in the background
  - internal change in protocol BAR/BARControl
  - fixed some lost strings
  - improved speed: inline some function calls
  - BARControl: added version number in about-dialog
  - fixed parsing of schedule data (type was missing)
  - fixed detection of size of current archive (was always 0)
  - new scheme to build a incremental file name from an
    archive file name if no incremental file name is given:
    - discard all %%-macros
    - discard all #
    - remove - and _ between macros
    - replace file name extension .bar by .bid
    A name like backup/system-%%type-%%a-####%%last.bar will
    be transformed to backup/system.bid
  - improved error messages
  - fixed debug function in string library: limit number of
    entries in string-free-list.
  - fixed problem with "broken pipe" error in network code
  - added support for openssl command in bar-keygen
  - fixed generating Java SSL key
  - added creating RPM and DEB packages

* Sat Feb 14 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.10a
  - fixed layout of login dialog
  - fixed missing BARControl.bat.in in distribution

* Wed Feb 11 2009 Torsten Rupp <torsten.rupp@gmx.net> 0.10
  - fixed some not freed resources
  - fixed error message when password is not given or wrong
  - improved input password: ask for password if not given
    in some configuration file when mode is "default"
  - added option --group: group equals files in list; limit
    output to most recent entries if not given option --all.
    Add archive name in group mode. Usage: find most recent
    file 'foo' in a set of backups, e. g.
  - .bar -# "*/foo" -g
  - added option --all: list all entries in group
  - BARControl: fixed Java exception when scheduling date/time
    can not be parsed
  - BARControl: added "normal" type to start-dialog
  - default value for ssh port is now 22
  - do not ask for a password if BAR is started server- or
    batch mode
  - BARControl: flush not written configuration data to disk
    when terminating BARControl
  - BARControl: fixed reseting port number
  - BARControl: set default sort mode to "weekday" for scheduling
    list
  - BARControl: try to login with preset server name and login
    password. Open login-dialog only when login fail or when option
    --login-dialog is given.
  - delete old archive files (only possible for file, ftp, sftp).
    Can be disabled by --keep-old-archive-files
  - BARControl: fixed bug when double-clicking file entry in file
  - fixed default login name/password for ftp, ssh: use command line
    options if not set otherwise
  - fixed default device name: use command line options if not set
    otherwise
  - fix problem with infinite backup when temporary directory is
    included in file list. The BAR temporary directory and all
    created files are now not included in a backup. Note: this cannot
    work when two instances of BAR are running doing a backup of
    each other.
  - added BAR man page (finally!)
  - improved GUI layout manager
  - added animated busy-dialogs to restore, added abort-function
  - added confirmation dialog to abort job button

* Wed Dec 10 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09i
  - fixed creating pid file: when server is detached write pid of
    running process, not of terminated parent (thanks to Matthias
    Albert)

* Tue Dec 09 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09h
  - fixed problem with internal data alignment on 64bit systems
  - added test data to distribution
  - added chunk BAR0 to archive files as indicator for a BAR file;
    chunk is skipped when reading an archive

* Tue Dec 09 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09g
  - added option --pid-file

* Sun Dec 07 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09f
  - small fix in "make install"

* Thu Dec 04 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09e
  - changed FTP URL to <login name>:<password>@<host name>/<filename>
  - improved password input: check if connection to ftp/ssh-server is
    possible in advance; select the right password
  - BARControl: change FTP field names, added password field
  - fixed problem when writing incremental file list and current
    directory is not writable: create the temporary file in the same
    directory like the destination file
  - removed some debug code
  - BARControl: fixed error handling when authorization fail
  - BARControl: fixed internal protocol data parser when data contain
    a negative number
  - fixed some wrong file names in "make install"
  - added BAR server start script for Debian (Thanks to Matthias Albert)
  - fixed lost string
  - run BAR as daemon in background; added option -D to disable detach
    mode
  - fixed missing server result when new job cannot be created

* Wed Dec 03 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09d
  - fixed missing server-jobs-directory in BAR server
  - fixed wrong path to bar+bar.cfg binary when installing
  - fix installation when BARControl was not built

* Mon Dec 01 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09c
  - BARControl: fixed #-parsing in archive name editor
  - fixed missing BARControl.in, BARControl.bat.in, BARControl.xml source
  - fixed creation of scripts from *.in files
  - completed bar-keygen
  - no external SWT jar needed anymore to compile (but can still be used
    optional)

* Thu Nov 27 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09b
  - fixed drag+drop in archive name editor
  - added crypt public key field in storage tab
  - added CRC to public/private-key to avoid SigSegV in gcrypt-library
    when key is invalid. Sorry this also mean the key data format
    changed, thus you have to generate a new public/private key pair.
    Note: you cannot open old archives with the new keys!
  - added option -h/--help to BARControl
  - BARControl: read default server name/password/ports from
    $HOME/.bar/BARControl.cfg (if this file exists)
  - renamed option --job-directory into --server-jobs-directory
  - improved check of options: output error if temporary directory
    cannot be written to
  - BARControl: add field for port to scp/sftp connections
  - change format for archive-file names. Use now URL like names:
    ftp://<name>@<host>/<filename>
    scp://<name>@<host>:<port>/<filename>
    sftp://<name>@<host>:<port>/<filename>
    dvd://<device>/<filename>
    device://<device>/<filename>
    <filename>
  - improved error messages
  - fixed linker problem: link libdl, too

* Fri Nov 21 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09a
  - small fixes in make files
  - fixes archive name generator: do not add part number of splitting
    of archive is not enabled
  - passwords can now also be read from a non-terminal input, e. g.
    a redirected file
  - fixed test

* Tue Nov 18 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.09
  - implemented a Java front end! This replaces the TclTk front end -
    please enjoy!
  - fixed bug in server password authorization: password was not
    checked until end

* Tue Sep 23 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.08a
  - fixed double usage of va_args variable
  - fixed problems with 64bit
  - fixed typing error in bar.cfg (thank to Matthias)

* Mon Sep 08 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.08
  - added asymmetric encryption with public-key
  - improved estimation of files/s, bytes/s, stored bytes/s and rest
    time
  - fixed input of password via SSH_ASKPASS
  - fixed exit code on create archive

* Sun Aug 17 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.07b
  - added option --long-format
  - added date/time to -l
  - fixed file filters "*" and "-! ..."

* Sun Jun 29 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.07a
  - fixed executing of external programs: handle signals
  - use /dev/dvd as default device for writing dvds
  - added storage bytes/s value to BARControl.tcl
  - implemented rename job in BARControl.tcl

* Sat May 24 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.07
  - integrated scheduler in BAR
  - added pause/continue of jobs
  - some internal bug fixes
  - improved response time of BARControl.tcl by read/write-locking
    of shared data structures in server
  - implemented archive name editor in BARControl.tcl
  - clean-up BARControl.tcl
  - added FTP protocol
  - changed some option names for better consistency

* Sat Mar 08 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.06
  - added option --debug to BARControl.tcl; output debug info
    in server
  - fixed memory leak in server
  - improved several error messages

* Mon Jan 21 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.05c
  - fixed SigSegV when reading broken/wrong archive
  - added retry (3) in storage via SSH

* Sat Jan 19 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.05b
  - fixed static linkage
  - support none bz2
  - TclTk TLS package is now optional
  - import support for non-ssh, non-crypt, non-bz2
  - added option --no-bar-on-dvd; store by default a copy of BAR
    executable on created DVDs
  - fixed wrong string when creating DVD

* Fri Jan 18 2008 Torsten Rupp <torsten.rupp@gmx.net> 0.05a
  - improved checks in configure
  - added configure option --disable-ssh
  - fixed static linkage

* Mon Dec 31 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.05
  - added macros %%type, %%last to archive file name
  - fixed path in bid-file name
  - removed option create-list-file; option --full create a list
    file, --incremental use the list and no option generate a
    standard archive (without an incremental list)
  - added options --full and --incremental to BARControl.tcl.
    With these options the settings in a configuration can be
    overwritten, e. g. useful to make incremental backups
    (define full-backup in configuration file; use --incremental
    to create a incremental backup on-the-fly)
  - do not write bid-file if archive cannot be stored
  - added file name editor in BARControl (check the "folder"-image
    right to the file name!)
  - fixed missing lock in debug-code of strings.c
  - renamed option --priority -> --nice-level; set nice level
    not thread priority
  - added password input dialog in case password does not match
    or no password is defined in the configuration files

* Tue Dec 18 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.04c
  - fixed creating of directories on restore of single files
  - fixed DVD burn commands (removed option "-dry-run" - sorry)

* Sun Dec 16 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.04b
  - fixed typing error in secure password memory

* Sun Dec 16 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.04a
  - fixed double free of a string
  - added check in string library for duplicate free
  - fixed abort
  - use base name of archive file name for incremental list file

* Sat Dec 15 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.04
  - added create-dialog to BARControl.tcl
  - added incremental backup
  - fixed bug in --request-volume-command (wrong command string)
  - fixed bug when executing external command (reading i/o)
  - fixed messages printed on console
  - replaced --enable-static-link by --enable-dynamic-link in
    configure and made static linkage to the default
  - added store/restore of special devices (character, block, fifo,
    socket)
  - fixed some small problems in restore
  - by the way: by accident BAR got his crucial test! I crashed
    my system partition and I could restore it with BAR!
    Nevertheless there is still a lot of work to do...

* Sat Dec 01 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.03a
  - fixed thread termination for "create" command

* Sat Dec 01 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.03
  - added option/configure value 'priority'
  - added log file support

* Wed Nov 28 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.02c
  - improved handling of DVDs
  - added configure to scanx TCL extension
  - improved making of distribution
  - added option --wait-first-volume

* Tue Nov 27 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.02b
  - improved handling of DVDs
  - started FAQ

* Mon Nov 26 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.02a
  - bug fix

* Sun Nov 25 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.02
  - implemented DVD support
  - fixed ssh connections
  - implemented more functions in BARControl.tcl
  - added install for scanx, mclistbox.tcl (needed for BARControl.tcl)
  - added config parser
  - added multiple server/device sections in config file
  - WARNING: changed usage of crypt password: encryption
    key is not filled with password anymore. Thus password
    "foo" is now different from "foofoo".
    Before upgrading BAR restore all archives!
  - added input of ssh/crypt password if not specified in config

* Thu Nov 01 2007 Torsten Rupp <torsten.rupp@gmx.net> 0.01
  - initial release

 -- Torsten Rupp <torsten.rupp@gmx.net>  Thu Nov 01 2007
