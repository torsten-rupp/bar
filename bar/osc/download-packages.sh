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
  echo "Usage: $0 <version>"
  exit 1
fi
version=$1

download "$OSC_URL/$OSC_PATH/CentOS_CentOS-5/i386"   'bar-.*\.rpm'     bar-$version-centos5_i386.rpm
download "$OSC_URL/$OSC_PATH/CentOS_CentOS-5/x86_64" 'bar-.*\.rpm'     bar-$version-centos5_x86_64.rpm
download "$OSC_URL/$OSC_PATH/CentOS_CentOS-6/i386"   'bar-.*\.rpm'     bar-$version-centos6_i386.rpm
download "$OSC_URL/$OSC_PATH/CentOS_CentOS-6/x86_64" 'bar-.*\.rpm'     bar-$version-centos6_x86_64.rpm

download "$OSC_URL/$OSC_PATH/Fedora_16/i386"         'bar-.*\.rpm'     bar-$version-fedora16_i386.rpm
download "$OSC_URL/$OSC_PATH/Fedora_16/x86_64"       'bar-.*\.rpm'     bar-$version-fedora16_x86_64.rpm
download "$OSC_URL/$OSC_PATH/Fedora_17/i386"         'bar-.*\.rpm'     bar-$version-fedora17_i386.rpm
download "$OSC_URL/$OSC_PATH/Fedora_17/x86_64"       'bar-.*\.rpm'     bar-$version-fedora17_x86_64.rpm

download "$OSC_URL/$OSC_PATH/RedHat_RHEL-5/i386"     'bar-.*\.rpm'     bar-$version-redhat5_i386.rpm
download "$OSC_URL/$OSC_PATH/RedHat_RHEL-5/x86_64"   'bar-.*\.rpm'     bar-$version-redhat5_x86_64.rpm
download "$OSC_URL/$OSC_PATH/RedHat_RHEL-6/i386"     'bar-.*\.rpm'     bar-$version-redhat6_i386.rpm
download "$OSC_URL/$OSC_PATH/RedHat_RHEL-6/x86_64"   'bar-.*\.rpm'     bar-$version-redhat6_x86_64.rpm

download "$OSC_URL/$OSC_PATH/SLE_11_SP1/i586"        'bar-.*\.rpm'     bar-$version-sle11_i586.rpm
download "$OSC_URL/$OSC_PATH/SLE_11_SP1/x86_64"      'bar-.*\.rpm'     bar-$version-sle11_x86_64.rpm

download "$OSC_URL/$OSC_PATH/openSUSE_11.4/i586"     'bar-.*\.rpm'     bar-$version-opensuse11.4_i586.rpm
download "$OSC_URL/$OSC_PATH/openSUSE_11.4/x86_64"   'bar-.*\.rpm'     bar-$version-opensuse11.4_x86_64.rpm
download "$OSC_URL/$OSC_PATH/openSUSE_12.1/i586"     'bar-.*\.rpm'     bar-$version-opensuse12.1_i586.rpm
download "$OSC_URL/$OSC_PATH/openSUSE_12.1/x86_64"   'bar-.*\.rpm'     bar-$version-opensuse12.1_x86_64.rpm
download "$OSC_URL/$OSC_PATH/openSUSE_12.2/i586"     'bar-.*\.rpm'     bar-$version-opensuse12.2_i586.rpm
download "$OSC_URL/$OSC_PATH/openSUSE_12.2/x86_64"   'bar-.*\.rpm'     bar-$version-opensuse12.2_x86_64.rpm

download "$OSC_URL/$OSC_PATH/Debian_5.0/i386"        'bar_.*\.deb'     bar-$version-debian5_i386.deb
download "$OSC_URL/$OSC_PATH/Debian_5.0/i386"        'bar-gui_.*\.deb' bar-gui-$version-debian5_i386.deb
download "$OSC_URL/$OSC_PATH/Debian_5.0/amd64"       'bar_.*\.deb'     bar-$version-debian5_amd64.deb
download "$OSC_URL/$OSC_PATH/Debian_5.0/amd64"       'bar-gui_.*\.deb' bar-gui-$version-debian5_amd64.deb
download "$OSC_URL/$OSC_PATH/Debian_6.0/i386"        'bar_.*\.deb'     bar-$version-debian6_i386.deb
download "$OSC_URL/$OSC_PATH/Debian_6.0/i386"        'bar-gui_.*\.deb' bar-gui-$version-debian6_i386.deb
download "$OSC_URL/$OSC_PATH/Debian_6.0/amd64"       'bar_.*\.deb'     bar-$version-debian6_amd64.deb
download "$OSC_URL/$OSC_PATH/Debian_6.0/amd64"       'bar-gui_.*\.deb' bar-gui-$version-debian6_amd64.deb

download "$OSC_URL/$OSC_PATH/xUbuntu_10.04/i386"     'bar_.*\.deb'     bar-$version-ubuntu10.04_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_10.04/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu10.04_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_10.04/amd64"    'bar_.*\.deb'     bar-$version-ubuntu10.04_amd64.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_10.04/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu10.04_amd64.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_11.04/i386"     'bar_.*\.deb'     bar-$version-ubuntu11.04_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_11.04/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu11.04_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_11.04/amd64"    'bar_.*\.deb'     bar-$version-ubuntu11.04_amd64.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_11.04/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu11.04_amd64.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_11.10/i386"     'bar_.*\.deb'     bar-$version-ubuntu11.10_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_11.10/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu11.10_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_11.10/amd64"    'bar_.*\.deb'     bar-$version-ubuntu11.10_amd64.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_11.10/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu11.10_amd64.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_12.04/i386"     'bar_.*\.deb'     bar-$version-ubuntu12.04_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_12.04/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu12.04_i386.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_12.04/amd64"    'bar_.*\.deb'     bar-$version-ubuntu12.04_amd64.deb
download "$OSC_URL/$OSC_PATH/xUbuntu_12.04/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu12.04_amd64.deb

md5sum bar-*$version*

exit 0
