# norootforbuild

Name:          bar
Version:       0.19
Release:       0
Summary:       Backup ARchiver
Source:        http://www.kigen.de/projects/bar/bar-%{version}.tar.bz2
URL:           http://www.kigen.de/projects/bar/index.html
Group:         Tool
License:       GPL v2
BuildRoot:     %{_tmppath}/build-%{name}-%{version}

BuildRequires: gcc gcc-c++ glibc-devel make
BuildRequires: jre >= 1.6.0
BuildRequires: java-devel >= 1.6.0
BuildRequires: tar
BuildRequires: unzip
BuildRequires: xz
BuildRequires: patch
BuildRequires: m4
BuildRequires: e2fsprogs

%description
BAR is backup archiver program. It can create compressed, encrypted
and splitted archives of files and harddisk images which can be
stored on a harddisk, cd, dvd, bd or directly on a server via ftp,
scp or sftp. A server-mode and a scheduler is integrated for making
automated backups in the background.

%prep
%setup -q

%build
mkdir packages
(
  cp %{_sourcedir}/zlib-1.2.5.tar.gz packages
  (cd packages; tar xzf zlib-1.2.5.tar.gz)
  ln -s packages/zlib-1.2.5 zlib
)
(
  cp %{_sourcedir}/bzip2-1.0.5.tar.gz packages
  (cd packages; tar xzf bzip2-1.0.5.tar.gz)
  ln -s packages/bzip2-1.0.5 bzip2
)
(
  cp %{_sourcedir}/xz-5.0.4.tar.gz packages
  (cd packages; tar xzf xz-5.0.4.tar.gz)
  ln -s packages/xz-5.0.4 xz
)
(
  cp %{_sourcedir}/lzo-2.06.tar.gz packages
  (cd packages; tar xzf lzo-2.06.tar.gz)
  ln -s packages/lzo-2.06 lzo
)
(
  cp %{_sourcedir}/lz4-r123.tar.gz packages
  (cd packages; tar xzf lz4-r123.tar.gz)
  ln -s packages/lz4-r123 lz4
)
(
  cp %{_sourcedir}/xdelta3.0.0.tar.gz packages
  (cd packages; tar xzf xdelta3.0.0.tar.gz)
  ln -s packages/xdelta3.0.0 xdelta3
)
(
  cp %{_sourcedir}/libgpg-error-1.7.tar.bz2 packages
  cp %{_sourcedir}/libgcrypt-1.4.4.tar.bz2 packages
  (cd packages; tar xjf libgpg-error-1.7.tar.bz2)
  (cd packages; tar xjf libgcrypt-1.4.4.tar.bz2)
  ln -s packages/libgpg-error-1.7 libgpg-error
  ln -s packages/libgcrypt-1.4.4 libgcrypt
)
(
  cp %{_sourcedir}/ftplib-3.1-src.tar.gz packages
  (cd packages; tar xzf ftplib-3.1-src.tar.gz)
  (cd packages/ftplib-3.1; patch -p3 < %{_sourcedir}/ftplib-3.1-1.patch)
  ln -s packages/ftplib-3.1 ftplib
)
(
  cp %{_sourcedir}/libssh2-1.4.2.tar.gz packages
  (cd packages; tar xzf libssh2-1.4.2.tar.gz)
  ln -s packages/libssh2-1.4.2 libssh2
)
(
  cp %{_sourcedir}/gnutls-3.1.18.tar.xz packages
  (cd packages; xz -d -c gnutls-3.1.18.tar.xz | tar xf -)
  ln -s packages/gnutls-3.1.18 gnutls
)
(
  cp %{_sourcedir}/libcdio-0.92.tar.gz packages
  (cd packages; tar xzf libcdio-0.92.tar.gz)
  ln -s packages/libcdio-0.92 libcdio
)

%configure --enable-package-check
%{__make} OPTFLAGS="%{optflags}"

%install
%makeinstall DIST=1 SYSTEM=Mandrake

%clean
%__rm -rf "%{buildroot}"

%check
%{__make} test1 test2 test3 test5 COMPRESS_NAMES_LZMA="lzma1 lzma2 lzma3 lzma4 lzma5 lzma6 lzma7 lzma8"

%files

%defattr(-,root,root)

%{_bindir}/bar
%{_bindir}/bar-debug
%{_bindir}/barcontrol
%{_bindir}/barcontrol-linux.jar
%{_bindir}/barcontrol-linux_64.jar
%{_bindir}/bar-keygen
/etc/init.d/barserver

%dir /etc/bar
%dir %attr(0700,root,root) /etc/bar/jobs

%config(noreplace) %attr(0600,root,root) /etc/bar/bar.cfg

%dir %{_datadir}/locale/jp
%dir %{_datadir}/locale/jp/LC_MESSAGES
%{_datadir}/locale/de/LC_MESSAGES/%{name}.mo
%{_datadir}/locale/jp/LC_MESSAGES/%{name}.mo

%doc ChangeLog doc/README
%doc doc/bar.pdf
%doc %{_mandir}/man7/bar.7.gz

%changelog
