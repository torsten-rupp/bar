# norootforbuild

Name:          bar
Version:       0.19
Release:       53.1
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
  cp %{_sourcedir}/xdelta*.tar.gz packages
  (cd packages; tar xzf xdelta*.tar.gz)
  ln -s `find packages -type d -name 'xdelta*'|head -1` xdelta3
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

%configure --enable-package-check
%{__make} OPTFLAGS="%{optflags}"

%install
%makeinstall DIST=1 SYSTEM=SuSE

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
