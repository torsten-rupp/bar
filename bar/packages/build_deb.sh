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
#trap /bin/bash ERR
#set -e

# build deb
cd /tmp
tar xjf $BASE_PATH/$distribution
cd bar-*
install -d debian
install /media/home/packages/debian/changelog \
        /media/home/packages/debian/compat \
        /media/home/packages/debian/control \
        /media/home/packages/debian/copyright \
        /media/home/packages/debian/preinst \
        /media/home/packages/debian/postinst \
        /media/home/packages/debian/prerm \
        /media/home/packages/debian/postrm \
        /media/home/packages/debian/rules \
        debian
install /media/home/packages/debian/source/format \
        debian/source
#debuild \
#  -rfakeroot \
#  -e SOURCE_DIR=$BASE_PATH \
#  -e testsFlag=$testsFlag \
#  -us -uc
debuild \
  -e SOURCE_DIR=$BASE_PATH \
  -e testsFlag=$testsFlag \
  -us -uc

#  -i -us -uc -b

# get result
cp -f /tmp/bar_[0-9]*.deb     $BASE_PATH/bar-$suffix.deb
chown $userGroup $BASE_PATH/bar-$suffix.deb
cp -f /tmp/bar-gui_[0-9]*.deb $BASE_PATH/bar-gui-$suffix.deb
chown $userGroup $BASE_PATH/bar-gui-$suffix.deb

md5sum $BASE_PATH/bar-$suffix.deb
md5sum $BASE_PATH/bar-gui-$suffix.deb

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi
#/bin/bash
