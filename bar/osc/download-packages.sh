#!/bin/sh

OSC_URL="http://download.opensuse.org"
OSC_BAR="repositories/home:/torsten20:/BAR"

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

wget "$OSC_URL/$OSC_BAR/CentOS_CentOS-5/i386/bar-$version-$nb.1.i386.rpm"     -O bar-$version-centos5_i386.rpm
wget "$OSC_URL/$OSC_BAR/CentOS_CentOS-5/x86_64/bar-$version-$nb.1.x86_64.rpm" -O bar-$version-centos5_x86_64.rpm

wget "$OSC_URL/$OSC_BAR/Fedora_13/i386/bar-$version-$nb.1.i386.rpm"           -O bar-$version-fedora13_i386.rpm
wget "$OSC_URL/$OSC_BAR/Fedora_13/x86_64/bar-$version-$nb.1.x86_64.rpm"       -O bar-$version-fedora13_x86_64.rpm

wget "$OSC_URL/$OSC_BAR/RedHat_RHEL-5/i386/bar-$version-$nb.1.i386.rpm"       -O bar-$version-redhat5_i386.rpm
wget "$OSC_URL/$OSC_BAR/RedHat_RHEL-5/x86_64/bar-$version-$nb.1.x86_64.rpm"   -O bar-$version-redhat5_x86_64.rpm

wget "$OSC_URL/$OSC_BAR/SLE_11_SP1/i586/bar-$version-$nb.1.i586.rpm"          -O bar-$version-sle11_i586.rpm
wget "$OSC_URL/$OSC_BAR/SLE_11_SP1/x86_64/bar-$version-$nb.1.x86_64.rpm"      -O bar-$version-sle11_x86_64.rpm

wget "$OSC_URL/$OSC_BAR/openSUSE_11.3/i586/bar-$version-$nb.1.i586.rpm"       -O bar-$version-opensuse11.3_i586.rpm
wget "$OSC_URL/$OSC_BAR/openSUSE_11.3/x86_64/bar-$version-$nb.1.x86_64.rpm"   -O bar-$version-opensuse11.3_x86_64.rpm

wget "$OSC_URL/$OSC_BAR/Debian_5.0/i386/bar_${version}_i386.deb"              -O bar-$version-debian5_i386.deb
wget "$OSC_URL/$OSC_BAR/Debian_5.0/i386/bar-gui_${version}_i386.deb"          -O bar-gui-$version-debian5_i386.deb
wget "$OSC_URL/$OSC_BAR/Debian_5.0/amd64/bar_${version}_amd64.deb"            -O bar-$version-debian5_amd64.deb
wget "$OSC_URL/$OSC_BAR/Debian_5.0/amd64/bar-gui_${version}_amd64.deb"        -O bar-gui-$version-debian5_amd64.deb

wget "$OSC_URL/$OSC_BAR/xUbuntu_10.04/i386/bar_${version}_i386.deb"           -O bar-$version-ubuntu10_i386.deb
wget "$OSC_URL/$OSC_BAR/xUbuntu_10.04/i386/bar-gui_${version}_i386.deb"       -O bar-gui-$version-ubuntu10_i386.deb
wget "$OSC_URL/$OSC_BAR/xUbuntu_10.04/amd64/bar_${version}_amd64.deb"         -O bar-$version-ubuntu10_amd64.deb
wget "$OSC_URL/$OSC_BAR/xUbuntu_10.04/amd64/bar-gui_${version}_amd64.deb"     -O bar-gui-$version-ubuntu10_amd64.deb

md5sum bar-*$version*
