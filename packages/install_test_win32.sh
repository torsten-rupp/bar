#!/bin/bash

#set -x

OPENJDK_VERSION=20.0.2
OPENJDK_URL=https://download.java.net/java/GA/jdk20.0.2/6e380f22cbe7469fa75fb448bd903d8e/9/GPL/openjdk-20.0.2_windows-x64_bin.zip

# parse arugments
setupFile=""
debugFlag=0
helpFlag=0
n=0
while test $# != 0; do
  case $1 in
    -h | --help)
      helpFlag=1
      shift
      ;;
    -d | --debug)
      debugFlag=1
      shift
      ;;
    --)
      shift
      break
      ;;
    -*)
      $ECHO >&2 "ERROR: unknown option '$1'"
      exit 1
      ;;
    *)
      setupFile="$1"
      shift
      ;;
  esac
done
while test $# != 0; do
  setupFile="$1"
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <setup files>..."
  echo ""
  echo "Options:  -d|--debug  enable debugging"
  echo "          -h|--help   print help"
  exit 0
fi

# check arguments
if test -z "$setupFile"; then
  echo >&2 ERROR: no setup file given!
  exit 1
fi

# get temporary file
tmpFile=`mktemp /tmp/test_rpm-XXXXXX`
if test -z "$tmpFile"; then
  echo >&2 "ERROR: cannot create temporary file!"
  exit 1
fi

# install required base packages
echo -n "Update packages..."
DEBIAN_FRONTEND=noninteractive apt-get -yq update 1>/dev/null 2>>$tmpFile
DEBIAN_FRONTEND=noninteractive apt-get -yq install \
  gawk \
  iproute2 \
  wine64 \
  wget \
  unzip \
  xvfb \
  1>/dev/null 2>$tmpFile
if test $? -eq 0; then
  echo "OK"
else
  echo "FAIL"
  cat $tmpFile
  rm -f $tmpFile
  exit 1
fi

dpkg --add-architecture i386
DEBIAN_FRONTEND=noninteractive apt-get -yq update 1>/dev/null 2>>$tmpFile
DEBIAN_FRONTEND=noninteractive apt-get -yq install \
  wine32 \
  1>/dev/null 2>$tmpFile
if test $? -eq 0; then
  echo "OK"
else
  echo "FAIL"
  cat $tmpFile
  rm -f $tmpFile
  exit 1
fi

wget \
  --no-check-certificate \
  --quiet \
  --output-document /root/jdk-windows-x64_bin.zip \
  $OPENJDK_URL

# get tools
wine=`which wine-stable`
if test -z "$wine"; then
  echo >&2 ERROR: wine not found!
  if test -n "$wineDir"; then
    rm -rf $wineDir;
  fi
  exit 1
fi
wineboot=`which wineboot-stable`
if test -z "$wineboot"; then
  echo >&2 ERROR: wineboot not found!
  if test -n "$wineDir"; then
    rm -rf $wineDir;
  fi
  exit 1
fi
winepath=`which winepath-stable`
if test -z "$winepath"; then
  echo >&2 ERROR: winepath not found!
  if test -n "$wineDir"; then
    rm -rf $wineDir;
  fi
  exit 1
fi

# install OpenJDK
(install -d /root/.wine/drive_c; cd /root/.wine/drive_c; unzip /root/jdk-windows-x64_bin.zip 1>/dev/null)

# set error handler: execute bash shell
trap /bin/bash ERR
set -e

# get unused local server port
read minPort maxPort < /proc/sys/net/ipv4/ip_local_port_range
testPort=`comm -23 <(seq $minPort $maxPort | sort) <(ss -Htan | awk '{print $4}' | cut -d':' -f2 | sort -u) | shuf | head -n 1`


if true; then
# TODO: wineboot cause a fork bomb with Jenkins - why?

# Notes:
#   * run with X11 server, do not expand variables, but pass OPENJDK_VERSION, setupFile, testPort
OPENJDK_VERSION=$OPENJDK_VERSION setupFile=$setupFile testPort=$testPort /usr/bin/xvfb-run -e /dev/stdout --auto-servernum sh <<"EOT"
WINEPREFIX=/root/.wine wineboot --init

# get paths
barWin32Path="C:/BAR"
jdkWin32Path="C:/jdk-$OPENJDK_VERSION/bin"
barUnixPath=`WINEPREFIX=/root/.wine winepath $barWin32Path`
jdkUnixPath=`WINEPREFIX=/root/.wine winepath $jdkWin32Path`

# setup
WINEPREFIX=/root/.wine wine $setupFile /DIR=$barWin32Path /VERYSILENT /SUPPRESSMSGBOXES

# simple command test
WINEPREFIX=/root/.wine wine "$barUnixPath/bar.exe" --version >/dev/null
WINEPREFIX=/root/.wine wine "$barUnixPath/bar.exe" --help >/dev/null
WINEPATH=$jdkUnixPath WINEPREFIX=/root/.wine wine cmd.exe /C "$barWin32Path/barcontrol.cmd" --help >/dev/null

# simple server test (Note: kill existing instance; systemd may not work inside docker)
(killall bar.exe 2>/dev/null || true)
WINEPREFIX=/root/.wine wine "$barUnixPath/bar.exe" --server --server-mode=master --server-port=$testPort &
sleep 20
WINEPATH=$jdkUnixPath WINEPREFIX=/root/.wine wine cmd.exe /C "$barWin32Path/barcontrol.cmd" --ping --port=$testPort
WINEPATH=$jdkUnixPath WINEPREFIX=/root/.wine wine cmd.exe /C "$barWin32Path/barcontrol.cmd" --list --port=$testPort
(killall bar.exe 2>/dev/null || true)

WINEPREFIX=/root/.wine wineboot --end-session
EOT
fi

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi

# free resources
rm -f $tmpFile

exit 0
