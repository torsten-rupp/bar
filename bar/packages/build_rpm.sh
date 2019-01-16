#!/bin/bash

#set -x

BASE_PATH=/media/home

# parse arugments
distribution=""
suffix=""
userGroup=""
testsFlag=0
debugFlag=0
helpFlag=0
n=0
while test $# != 0; do
  case $1 in
    -h | --help)
      helpFlag=1
      shift
      ;;
    -t | --tests)
      testsFlag=1
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
          distribution="$1"
          n=1
          ;;
        1)
          suffix="$1"
          n=2
          ;;
        2)
          userGroup="$1"
          n=3
          ;;
      esac
      shift
      ;;
  esac
done
while test $# != 0; do
  case $n in
    0)
      distribution="$1"
      n=1
      ;;
    1)
      suffix="$1"
      n=2
      ;;
    2)
      userGroup="$1"
      n=3
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <distribution> <suffix> <user:group>"
  echo ""
  echo "Options:  -t|--test  execute tests"
  echo "          -h|--help  print help"
  exit 0
fi

# check arguments
if test -z "$distribution"; then
  echo >&2 ERROR: no distribution name given!
  exit 1
fi
if test -z "$suffix"; then
  echo >&2 ERROR: no suffix given!
  exit 1
fi
if test -z "$userGroup"; then
  echo >&2 ERROR: no user:group id given!
  exit 1
fi

# set error handler: execute bash shell
trap /bin/bash ERR
set -e

# build rpm
cd /tmp
tar xjf $BASE_PATH/$distribution
cd bar-*
rpmbuild \
  -bb \
  --define "_sourcedir $BASE_PATH" \
  --define "testsFlag $testsFlag" \
  $BASE_PATH/packages/bar.spec

# get result
cp -f /root/rpmbuild/RPMS/*/bar-[0-9]*.rpm $BASE_PATH/bar-$suffix.rpm
chown $userGroup $BASE_PATH/bar-$suffix.rpm

md5sum $BASE_PATH/bar-$suffix.rpm

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi
#/bin/bash
