# norootforbuild

Name:          %{packageName}
Version:       %{version}
Release:       0
Summary:       Backup ARchiver
Source:        http://www.kigen.de/projects/bar/%{distributionFileName}
URL:           http://www.kigen.de/projects/bar/index.html
Group:         Tool
License:       GPL-2.0
BuildRoot:     %{_tmppath}/build-%{name}-%{version}

BuildRequires: curl
BuildRequires: bc
BuildRequires: coreutils
BuildRequires: e2fsprogs
BuildRequires: make
BuildRequires: gcc gcc-c++ glibc-devel binutils
BuildRequires: java-1.8.0-openjdk-devel
BuildRequires: jre >= 1.8.0
BuildRequires: lua
BuildRequires: m4
BuildRequires: openssl
BuildRequires: patch
BuildRequires: tar
BuildRequires: tcl
BuildRequires: unzip
BuildRequires: wget
BuildRequires: xz
BuildRequires: initscripts

# Note: no JRE; installation without JRE should be possible
# Requires: jre
Requires: openssl

%description
BAR is backup archiver program. It can create compressed, encrypted
and split archives of files and harddisk images which can be
stored on a harddisk, cd, dvd, bd or directly on a server via ftp,
scp, sftp or webdav. A server-mode and a scheduler is integrated for
creating automated backups in the background.

#define ADDITIONAL_DOWNLOAD_FLAGS zlib icu
%define ADDITIONAL_DOWNLOAD_FLAGS %{nil}
#define ADDITIONAL_CONFIGURE_FLAGS --disable-bz2 --disable-lzma --disable-xz --disable-lzo --disable-lz4 --disable-zstd --disable-xdelta3 --disable-gcrypt --disable-curl --disable-ssh --disable-tls --disable-iso9660 --disable-pcre
%define ADDITIONAL_CONFIGURE_FLAGS %{nil}

%prep
%setup -q -n %{packageName}-%{version}
#echo _sourcedir=%_sourcedir
#echo version=%{version}
#echo testsFlag=%{testsFlag}
#echo packageName=%{packageName}

%build
sh ./download-third-party-packages.sh \
  --local-directory /media/extern \
  --no-verbose \
  %{ADDITIONAL_DOWNLOAD_FLAGS}
%configure  \
  --enable-extern-check \
  %ADDITIONAL_CONFIGURE_FLAGS
%{__make} OPTFLAGS="%{optflags}" -C bar %{?_smp_mflags} all
%{__make} OPTFLAGS="%{optflags}" all

%install
%makeinstall DESTDIR=%{buildroot} DIST=1 SYSTEM=CentOS SYSTEM_INIT=systemd

%clean
# Note: keep build directory for debugging purposes
#%__rm -rf "%{_builddir}"

%check
if test %{testsFlag} -eq 1; then
  %{__make} test1-debug test2-debug test3-debug test4-debug test5-debug \
    OPTIONS="--no-stop-on-attribute-error" \
    COMPRESS_NAMES_LZMA="lzma1 lzma2 lzma3 lzma4 lzma5 lzma6 lzma7";
fi

%pre
# try to stop BAR server service
if test -d %{_prefix}/lib/systemd; then
  type chkconfig 1>/dev/null 2>/dev/null
  if test $? -eq 0; then
    chkconfig barserver off 1>/dev/null 2>/dev/null || true
  fi
  type systemctl 1>/dev/null 2>/dev/null
  if test $? -eq 0; then
    systemctl stop barserver 1>/dev/null 2>/dev/null || true
  fi
  type service 1>/dev/null 2>/dev/null
  if test $? -eq 0; then
    service barserver stop 1>/dev/null 2>/dev/null || true
  fi
else
  %{_sysconfdir}/init.d/barserver stop 1>/dev/null 2>/dev/null || true
fi

%post

# Note: use %posttrans instead of %post as workaround for wrong %preun of old package
%posttrans
chmod 700 %{_sysconfdir}/bar
chmod 600 %{_sysconfdir}/bar/bar.cfg

# install BAR service
if test -d /lib/systemd; then
  if test ! -f /lib/systemd/system/barserver.service; then
    install -d /lib/systemd/system
    install -m 644 /var/lib/bar/install/barserver.service /lib/systemd/system
  fi
fi

# install init.d script
install -d %{_sysconfdir}/init.d;
if   test -f %{_sysconfdir}/SuSE-release -o -d %{_sysconfdir}/SuSEconfig; then
  install -m 755 /var/lib/bar/install/barserver-SuSE %{_sysconfdir}/init.d/barserver
elif test -f %{_sysconfdir}/fedora-release; then
  install -m 755 /var/lib/bar/install/barserver-Fedora %{_sysconfdir}/init.d/barserver
elif test -f %{_sysconfdir}/redhat-release -a -n "`grep 'Red Hat' %{_sysconfdir}/redhat-release 2>/dev/null`"; then
  install -m 755 /var/lib/bar/install/barserver-RedHat %{_sysconfdir}/init.d/barserver
elif test -f %{_sysconfdir}/redhat-release -a -n "`grep 'CentOS' %{_sysconfdir}/redhat-release 2>/dev/null`"; then
  install -m 755 /var/lib/bar/install/barserver-CentOS %{_sysconfdir}/init.d/barserver
elif test -f %{_sysconfdir}/lsb-release; then
  install -m 755 /var/lib/bar/install/barserver-debian %{_sysconfdir}/init.d/barserver
elif test -f %{_sysconfdir}/debian_release; then
  install -m 755 /var/lib/bar/install/barserver-debian %{_sysconfdir}/init.d/barserver
else
  install -m 755 /var/lib/bar/install/barserver-debian %{_sysconfdir}/init.d/barserver
fi

# info to start BAR server service
if test -d /lib/systemd; then
  if test -n "`ps -p1|grep systemd`"; then
    systemctl daemon-reload
    systemctl enable barserver
    echo "Please start BAR server with 'service barserver start'"
  else
    echo >&2 "Warning: systemd not available or not started with systemd"
  fi
else
  echo "Please start BAR server with '%{_sysconfdir}/init.d/barserver start'"
 fi

# clean-up
rm -rf %{_tmppath}/bar

%preun
# stop BAR server service
if test -d %{_prefix}/lib/systemd; then
  type chkconfig 1>/dev/null 2>/dev/null
  if test $? -eq 0; then
    chkconfig barserver off
    if test $1 -gt 1; then
      chkconfig --del barserver
    fi
  fi
  type systemctl 1>/dev/null 2>/dev/null
  if test $? -eq 0; then
    systemctl stop barserver
    if test $1 -gt 1; then
      systemctl disable barserver
    fi
  fi
  type service 1>/dev/null 2>/dev/null
  if test $? -eq 0; then
    service barserver stop
  fi
else
  %{_sysconfdir}/init.d/barserver stop 1>/dev/null
fi

%postun
# remove BAR service/init.d script on uninstall
if test $1 -lt 1; then
  if test -d /lib/systemd/system; then
    if test -f /lib/systemd/system/barserver.service; then
      mv --backup=numbered /lib/systemd/system/barserver.service /lib/systemd/system/barserver.service.rpmsave
    fi
    rm -f /lib/systemd/system/barserver.service
  fi
  if test -f /etc/init.d/barserver; then
    mv --backup=numbered /etc/init.d/barserver /etc/init.d/barserver.rpmsave
  fi
  rm -f /etc/init.d/barserver
fi

%files

%defattr(-,root,root)

%{_bindir}/bar
%{_bindir}/bar.sym
%{_bindir}/bar-debug
%{_bindir}/bar-debug.sym
%{_bindir}/bar-index
%{_bindir}/bar-index-debug
%{_bindir}/barcontrol
%{_bindir}/barcontrol-linux.jar
%{_bindir}/barcontrol-linux_64.jar
%{_bindir}/bar-keygen

%dir %{_sysconfdir}/bar
%dir %attr(0700,root,root) %{_sysconfdir}/bar/jobs

%config(noreplace) %attr(0600,root,root) /etc/bar/bar.cfg
%config(noreplace) %attr(0600,root,root) /etc/logrotate.d/bar

%lang(de) %dir %{_datadir}/locale/jp
%lang(jp) %dir %{_datadir}/locale/jp/LC_MESSAGES
%lang(de) %{_datadir}/locale/de/LC_MESSAGES/bar.mo
%lang(jp) %{_datadir}/locale/jp/LC_MESSAGES/bar.mo

# BAR service/init.d scripts
/var/lib/bar/install/*

%doc ChangeLog doc/README
%doc doc/backup-archiver.pdf
%doc %{_mandir}/man7/bar.7.gz

%changelog
