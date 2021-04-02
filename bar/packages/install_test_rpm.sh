#!/bin/bash

BASE_PATH=/media/home

#set -x

# parse arugments
suffix=""
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
          suffix="$1"
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
      suffix="$1"
      n=1
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <version> <suffix>"
  echo ""
  echo "Options:  -d|--debug  enable debugging"
  echo "          -h|--help   print help"
  exit 0
fi

# check arguments
if test -z "$suffix"; then
  echo >&2 ERROR: no suffix given!
  exit 1
fi

# install required base packages
type yum 2>/dev/null
if test $? -eq 0; then
  yum -y update
  yum -y install initscripts
  yum -y install openssl jre
  yum -y install psmisc
fi
type zypper 2>/dev/null
if test $? -eq 0; then
  zypper -q update -y
  zypper -q install -y initscripts
  zypper -q install -y openssl jre
  zypper -q install -y psmisc
fi

# set error handler: execute bash shell
trap /bin/bash ERR
set -e

# install rpm
rpm -i $BASE_PATH/backup-archiver-$suffix.rpm

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
