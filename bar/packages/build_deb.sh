#!/bin/bash

#set -x

BASE_PATH=/media/home

# parse arugments
packageName=""
distributionFileName=""
version=""
userGroup=""
debFileName=""
debFileNameGUI=""
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
          packageName="$1"
          n=1
          ;;
        1)
          distributionFileName="$1"
          n=2
          ;;
        2)
          version="$1"
          n=3
          ;;
        3)
          userGroup="$1"
          n=4
          ;;
        4)
          debFileName="$1"
          n=5
          ;;
        5)
          debFileNameGUI="$1"
          n=6
          ;;
      esac
      shift
      ;;
  esac
done
while test $# != 0; do
  case $n in
    0)
      packageName="$1"
      n=1
      ;;
    1)
      distributionFileName="$1"
      n=2
      ;;
    2)
      version="$1"
      n=3
      ;;
    3)
      userGroup="$1"
      n=4
      ;;
    4)
      debFileName="$1"
      n=5
      ;;
    5)
      debFileNameGUI="$1"
      n=6
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <distribution name> <version> <user:group> <package name> [<package name GUI>]"
  echo ""
  echo "Options:  -t|--test  execute tests"
  echo "          -h|--help  print help"
  exit 0
fi

# check arguments
if test -z "$packageName"; then
  echo >&2 ERROR: no package name given!
  exit 1
fi
if test -z "$distributionFileName"; then
  echo >&2 ERROR: no distribution filename given!
  exit 1
fi
if test -z "$version"; then
  echo >&2 ERROR: no version given!
  exit 1
fi
if test -z "$userGroup"; then
  echo >&2 ERROR: no user:group id given!
  exit 1
fi
if test -z "$debFileName"; then
  echo >&2 ERROR: no DEB filename given!
  exit 1
fi

# set error handler: execute bash shell
#trap /bin/bash ERR
#set -e

# build deb
cd /tmp
tar xjf $BASE_PATH/$distributionFileName
cd $packageName-$version
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
  -e packageName=$packageName \
  -e distributionFileName=$distributionFileName \
  -e version=$version \
  -e debFileName=$debFileName \
  -e testsFlag=$testsFlag \
  -us -uc
#  -i -us -uc -b

# get result
cp -f /tmp/${packageName}_[0-9]*.deb     $BASE_PATH/$debFileName
chown $userGroup $BASE_PATH/$debFileName
if test -n "$debFileNameGUI"; then
  cp -f /tmp/${packageName}-gui_[0-9]*.deb $BASE_PATH/$debFileNameGUI
  chown $userGroup $BASE_PATH/$debFileNameGUI
fi

md5sum $BASE_PATH/$debFileName
if test -n "$debFileNameGUI"; then
  md5sum $BASE_PATH/$debFileNameGUI
fi

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi
/bin/bash
