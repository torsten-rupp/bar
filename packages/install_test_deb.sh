#!/bin/bash

#set -x

# parse arugments
debFiles=""
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
      debFiles="$debFiles $1"
      shift
      ;;
  esac
done
while test $# != 0; do
  debFiles="$debFiles $1"
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <DEB files>..."
  echo ""
  echo "Options:  -d|--debug  enable debugging"
  echo "          -h|--help   print help"
  exit 0
fi

# check arguments
if test -z "$debFiles"; then
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
DEBIAN_FRONTEND=noninteractive apt-get -yq update 1>/dev/null 2>/dev/null
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

# install/upgrade packages
apt update 1>/dev/null 2>/dev/null
apt -y upgrade --fix-missing 1>/dev/null 2>/dev/null
apt -y install --fix-missing \
  procps 1>/dev/null 2>/dev/null

# install deb
dpkg -i $debFiles

# simple command test
bar --version 1>/dev/null
bar --help 1>/dev/null
barcontrol --help 1>/dev/null

# simple server test (Note: kill existing instance and ignore SIGTERM; systemd may not work inside docker)
trap '' SIGTERM
(killall bar 2>/dev/null || true)
trap - SIGTERM
bar-debug --server --debug-systemd --debug-run-time=60
bar --server &
pid=$!
sleep 20
barcontrol --ping
barcontrol --list
(kill $pid 2>/dev/null || true)

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi

# free resources
rm -f $tmpFile

exit 0
