# norootforbuild

Name:          bar
Version:       0.17
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
BuildRequires: gnutls
BuildRequires: openssl
BuildRequires: openssl-devel
BuildRequires: tar
BuildRequires: unzip
BuildRequires: patch

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
  cp %{_sourcedir}/zlib-1.2.7.tar.gz packages
  (cd packages; tar xzf zlib-1.2.7.tar.gz)
  ln -s packages/zlib-1.2.7 zlib
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
  cp %{_sourcedir}/gnutls-2.10.2.tar.bz2 packages
  (cd packages; tar xjf gnutls-2.10.2.tar.bz2)
  ln -s packages/gnutls-2.10.2 gnutls
)
(
  cp %{_sourcedir}/libcdio-0.82.tar.gz packages
  (cd packages; tar xzf libcdio-0.82.tar.gz)
  ln -s packages/libcdio-0.82 libcdio
)
%configure
%{__make} OPTFLAGS="%{optflags}"

%install
%makeinstall DIST=1 SYSTEM=Fedora

%clean
%__rm -rf "%{buildroot}"

%files
%defattr(-,root,root)
%{_bindir}/bar
%{_bindir}/barcontrol
%{_bindir}/barcontrol-linux.jar
%{_bindir}/barcontrol-linux_64.jar
%{_bindir}/bar-keygen
%doc ChangeLog doc/README doc/bar.pdf
%{_mandir}/man7/bar.7.gz
%dir /etc/bar
%config /etc/bar/bar.cfg
/etc/init.d/barserver

%changelog
