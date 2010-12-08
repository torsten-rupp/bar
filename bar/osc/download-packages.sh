#!/bin/sh

if test -z "$1"; then
  echo >&2 "ERROR: no version specified!"
  echo "Usage: $0 <version> <nb>"
  exit 1
fi
if test -z "$2"; then
  echo >&2 "ERROR: no number specified!"
  echo "Usage: $0 <version> <nb>"
  exit 1
fi

version=$1
nb=$2

wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/CentOS_CentOS-5/i386/bar-$version-$nb.1.i386.rpm"     -O bar-$version-centos5_i386.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/CentOS_CentOS-5/x86_64/bar-$version-$nb.1.x86_64.rpm" -O bar-$version-centos5_x86_64.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/Fedora_13/i386/bar-$version-$nb.1.i386.rpm"           -O bar-$version-fedora13_i386.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/Fedora_13/x86_64/bar-$version-$nb.1.x86_64.rpm"       -O bar-$version-fedora13_x86_64.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/RedHat_RHEL-5/i386/bar-$version-$nb.1.i386.rpm"       -O bar-$version-redhat5_i386.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/RedHat_RHEL-5/x86_64/bar-$version-$nb.1.x86_64.rpm"   -O bar-$version-redhat5_x86_64.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/SLE_11_SP1/i586/bar-$version-$nb.1.i586.rpm"          -O bar-$version-sle11_i586.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/SLE_11_SP1/x86_64/bar-$version-$nb.1.x86_64.rpm"      -O bar-$version-sle11_x86_64.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/openSUSE_11.3/i586/bar-$version-$nb.1.i586.rpm"       -O bar-$version-opensuse11.3_i586.rpm
wget "http://download.opensuse.org/repositories/home:/torsten20:/BAR/openSUSE_11.3/x86_64/bar-$version-$nb.1.x86_64.rpm"   -O bar-$version-opensuse11.3_x86_64.rpm
