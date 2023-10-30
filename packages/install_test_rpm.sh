#!/bin/bash

#set -x

# parse arugments
rpmFiles=""
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
      rpmFiles="$rpmFiles $1"
      shift
      ;;
  esac
done
while test $# != 0; do
  rpmFiles="$rpmFiles $1"
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <RPM files>..."
  echo ""
  echo "Options:  -d|--debug  enable debugging"
  echo "          -h|--help   print help"
  exit 0
fi

pwd
ls -la

# check arguments
if test -z "$rpmFiles"; then
  echo >&2 ERROR: no RPM files given!
  exit 1
fi

# get temporary file
tmpFile=`mktemp /tmp/test_rpm-XXXXXX`
if test -z "$tmpFile"; then
  echo >&2 "ERROR: cannot create temporary file!"
  exit 1
fi

# install required base packages
type yum 1>/dev/null 2>/dev/null
if test $? -eq 0; then
  echo -n "Update packages..."

  # fix CentOS repositories
  if test -n "/etc/yum.repos.d/CentOS-*"; then
    sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-*
    sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*
  fi

  yum -y update 1>/dev/null 2>/dev/null
  yum -y install \
    initscripts \
    openssl \
    jre \
    psmisc \
    1>/dev/null 2>$tmpFile
  if test $? -eq 0; then
    echo "OK"
  else
    echo "FAIL"
    cat $tmpFile
    rm -f $tmpFile
    exit 1
  fi
fi
type zypper 1>/dev/null 2>/dev/null
if test $? -eq 0; then
  echo -n "Update packages..."
  zypper -q update -y  1>/dev/null 2>/dev/null
  zypper -q install -y \
    openssl \
    jre \
    psmisc \
    1>/dev/null 2>$tmpFile
  if test $? -eq 0; then
    echo "OK"
  else
    echo "FAIL"
    cat $tmpFile
    rm -f $tmpFile
    exit 1
  fi
fi

# set error handler: execute bash shell
trap /bin/bash ERR
set -e

# install rpm
rpm -i $rpmFiles

# simple command test
bar --version 1>/dev/null
bar --help 1>/dev/null
barcontrol --help 1>/dev/null

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
