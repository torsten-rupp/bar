#!/bin/bash

#set -x

BASE_PATH=/media/home

# parse arugments
debFileSuffix=""
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
      case $n in
        0)
          debFileSuffix="$1"
          n=1
          ;;
      esac
      shift
      ;;
  esac
done
while test $# != 0; do
  case $n in
    0)
      debFileSuffix="$1"
      n=1
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <DEB file suffix>"
  echo ""
  echo "Options:  -d|--debug  enable debugging"
  echo "          -h|--help   print help"
  exit 0
fi

# check arguments
if test -z "$debFileSuffix"; then
  echo >&2 ERROR: no DEB file suffix given!
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
  default-jre-headless \
  systemd \
  1>/dev/null 2>$tmpFile
if test $? -eq 0; then
  echo "OK"
else
  echo "FAIL"
  cat $tmpFile
  rm -f $tmpFile
  exit 1
fi

# set error handler: execute bash shell
trap /bin/bash ERR
set -e

# install deb
dpkg -i $BASE_PATH/backup-archiver-$debFileSuffix.deb
dpkg -i $BASE_PATH/backup-archiver-gui-$debFileSuffix.deb

# simple command test
bar --version
bar --help >/dev/null
barcontrol --help >/dev/null

# simple server test (Note: kill existing instance; systemd may not work inside docker)
(killall bar 2>/dev/null || true)
bar --daemon
sleep 20
barcontrol --ping
barcontrol --list
(killall bar 2>/dev/null || true)

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi

# free resources
rm -f $tmpFile

exit 0
