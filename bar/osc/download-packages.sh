#!/bin/sh

OSC_URL="http://download.opensuse.org"
OSC_PATH="repositories/home:/torsten20:/BAR"

download()
{
  local path="$1"; shift
  local pattern="$1"; shift
  local destinationFileName="$1"; shift

  fileName=`wget -q -O - "$path"|grep -E -e \"$pattern\"|head -1|sed "s/.*\($pattern\).*/\1/g"`
#  echo $fileName
  wget "$path/$fileName" -O $destinationFileName
}

if test -z "$1"; then
  echo >&2 "ERROR: no version specified!"
  echo "Usage: $0 <version> <nb>"
  exit 1
fi
version=$1

download "$OSC_URL/$OSC_PATH/CentOS_CentOS-5/i386"   'bar-.*\.rpm'     bar-$version-centos5_i386.rpm
download "$OSC_URL/$OSC_PATH/CentOS_CentOS-5/x86_64" 'bar-.*\.rpm'     bar-$version-centos5_x86_64.rpm

download "$OSC_URL/$OSC_PATH/Fedora_13/i386"         'bar-.*\.rpm'     bar-$version-fedora13_i386.rpm
download "$OSC_URL/$OSC_PATH/Fedora_13/x86_64"       'bar-.*\.rpm'     bar-$version-fedora13_x86_64.rpm

download "$OSC_URL/$OSC_PATH/RedHat_RHEL-5/i386"     'bar-.*\.rpm'     bar-$version-redhat5_i386.rpm
download "$OSC_URL/$OSC_PATH/RedHat_RHEL-5/x86_64"   'bar-.*\.rpm'     bar-$version-redhat5_x86_64.rpm

download "$OSC_URL/$OSC_PATH/SLE_11_SP1/i586"        'bar-.*\.rpm'     bar-$version-sle11_i586.rpm
download "$OSC_URL/$OSC_PATH/SLE_11_SP1/x86_64"      'bar-.*\.rpm'     bar-$version-sle11_x86_64.rpm

download "$OSC_URL/$OSC_PATH/openSUSE_11.3/i586"     'bar-.*\.rpm'     bar-$version-opensuse11.3_i586.rpm
download "$OSC_URL/$OSC_PATH/openSUSE_11.3/x86_64"   'bar-.*\.rpm'     bar-$version-opensuse11.3_x86_64.rpm

download "$OSC_URL/$OSC_PATH/Debian_5.0/i386"        'bar_.*\.deb'     bar-$version-debian5_i386.deb
download "$OSC_URL/$OSC_PATH/Debian_5.0/i386"        'bar-gui_.*\.deb' bar-gui-$version-debian5_i386.deb
download "$OSC_URL/$OSC_PATH/Debian_5.0/amd64"       'bar_.*\.deb'     bar-$version-debian5_amd64.deb
download "$OSC_URL/$OSC_PATH/Debian_5.0/amd64"       'bar-gui_.*\.deb' bar-gui-$version-debian5_amd64.deb

download "$OSC_URL/$OSC_PATH/xUbuntu_10.04/i386"     'bar_.*\.deb'     bar-$version-ubuntu10_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_10.04/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu10_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_10.04/amd64"    'bar_.*\.deb'     bar-$version-ubuntu10_amd64.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_10.04/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu10_amd64.deb

md5sum bar-*$version*

exit 0
